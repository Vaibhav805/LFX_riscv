#!/usr/bin/env python3
"""
openblas_ebpf_observe.py
========================
eBPF/bpftrace observability layer for OpenBLAS riscv64 under QEMU.

Covers:
  1. Thread-pool thrashing detection (sched_yield spike)
  2. Vector tail overhead estimation (vsetvli call frequency)
  3. Memory bandwidth tracing (mmap / brk patterns)
  4. QEMU vs real hardware behavioral delta

Usage:
  # Run during qemu-riscv64-static execution:
  sudo python3 openblas_ebpf_observe.py --pid $(pgrep qemu-riscv64) --duration 30

  # Or attach to a specific binary:
  sudo python3 openblas_ebpf_observe.py --cmd "qemu-riscv64-static ./dgemm_validate_riscv"
"""

import subprocess
import json
import time
import argparse
import sys
import os
from pathlib import Path
from dataclasses import dataclass, field, asdict
from typing import Optional


# ── eBPF probe definitions ────────────────────────────────────────────────────

# PROBE 1: Thread-pool thrashing detector
# Fires on every sched_yield syscall. A spike > 10k/sec indicates
# OpenBLAS's internal thread pool is busy-waiting under QEMU.
# Under QEMU, riscv64 threads cannot actually run in parallel —
# they serialize through the TCG translator, causing the thread pool
# to spin-yield waiting for work that never comes in parallel.
BPFTRACE_SCHED_YIELD = r"""
#!/usr/bin/env bpftrace
// Thread-pool thrashing detector
// High sched_yield rate = OpenBLAS thread pool spinning under QEMU

tracepoint:syscalls:sys_enter_sched_yield
/pid == TARGET_PID/
{
    @yield_count = count();
    @yield_ts[nsecs] = 1;
}

interval:s:1
{
    printf("YIELD_RATE: %lld/sec\n", @yield_count);
    clear(@yield_count);
}

END { clear(@yield_ts); }
"""

# PROBE 2: QEMU Vector Tail overhead estimator
# We cannot directly probe vsetvli (it's a riscv64 instruction, invisible
# to the x86 host kernel). Instead we proxy via:
#   - mmap calls (QEMU allocates per-thread vector register state)
#   - The ratio of futex:sched_yield tells us contention vs spinning
BPFTRACE_VECTOR_PROXY = r"""
#!/usr/bin/env bpftrace
// Vector context overhead proxy
// QEMU saves/restores riscv64 vector registers on context switches
// Each restore requires re-executing vsetvli to restore vl/vtype

// Track futex (real blocking) vs sched_yield (spinning)
tracepoint:syscalls:sys_enter_futex /pid == TARGET_PID/
{ @futex = count(); }

tracepoint:syscalls:sys_enter_sched_yield /pid == TARGET_PID/
{ @yield = count(); }

tracepoint:syscalls:sys_enter_mmap /pid == TARGET_PID/
{ @mmap = count(); @mmap_sizes = hist(args->len); }

interval:s:5
{
    printf("THRASH_RATIO futex=%lld yield=%lld mmap=%lld\n",
           @futex, @yield, @mmap);
    printf("  => vector_save_overhead_proxy: yield/futex = %.2f\n",
           (float)@yield / (@futex + 1));
    // Interpretation:
    //   ratio < 1.0  : healthy — threads blocking properly
    //   ratio 1-10   : mild spinning — acceptable under QEMU
    //   ratio > 100  : thread pool thrashing — set USE_THREAD=0
    clear(@futex); clear(@yield); clear(@mmap);
}
"""

# PROBE 3: Memory bandwidth pattern
# OpenBLAS dgemm uses a packing scheme: it copies A and B into L2-sized
# panels before the kernel loop. Under QEMU, this generates large
# sequential write bursts. We trace brk/mmap to profile allocation.
BPFTRACE_MEMORY = r"""
#!/usr/bin/env bpftrace
// OpenBLAS memory allocation tracer
// dgemm packs matrix panels into ~L2-cache-sized buffers
// Expected: 2-4 large mmap calls per dgemm invocation

tracepoint:syscalls:sys_enter_mmap /pid == TARGET_PID/
{
    printf("MMAP size=%lu prot=%d\n", args->len, args->prot);
    @alloc_hist = hist(args->len);
}

tracepoint:syscalls:sys_enter_munmap /pid == TARGET_PID/
{
    @free_count = count();
}

tracepoint:syscalls:sys_enter_brk /pid == TARGET_PID/
{
    printf("BRK addr=0x%lx\n", args->brk);
}
"""

# PROBE 4: syscount summary (the one you already ran for ARPACK-ng)
SYSCOUNT_CMD = [
    "syscount-bpfcc",
    "-p", "TARGET_PID",
    "--duration", "30",
    "-j"   # JSON output (if supported) or parse text
]


@dataclass
class ObservabilityResult:
    binary_name:     str
    duration_sec:    int
    yield_per_sec:   float = 0.0
    futex_count:     int   = 0
    yield_count:     int   = 0
    mmap_count:      int   = 0
    thrash_ratio:    float = 0.0
    thread_verdict:  str   = "UNKNOWN"  # HEALTHY / MILD_SPIN / THRASHING
    recommendation:  str   = ""
    raw_syscounts:   dict  = field(default_factory=dict)


class OpenBLASObserver:
    """
    Attaches eBPF probes to a qemu-riscv64-static process running
    an OpenBLAS-linked binary and diagnoses thread-pool and vector
    tail overhead.
    """

    THRASH_THRESHOLD_HIGH = 100.0   # yield/futex > 100 = thrashing
    THRASH_THRESHOLD_MILD = 10.0    # yield/futex 10-100 = mild spinning

    def __init__(self, pid: Optional[int] = None,
                 cmd: Optional[str] = None,
                 duration: int = 30):
        self.pid      = pid
        self.cmd      = cmd
        self.duration = duration

    def _check_bpftools(self) -> bool:
        for tool in ["syscount-bpfcc", "bpftrace"]:
            if subprocess.run(["which", tool], capture_output=True).returncode == 0:
                return True
        print("  [WARN] syscount-bpfcc / bpftrace not found.")
        print("  Install: sudo apt install bpfcc-tools bpftrace")
        return False

    def run_syscount(self, pid: int) -> dict:
        """Run syscount-bpfcc for `duration` seconds against pid."""
        print(f"  Running syscount-bpfcc -p {pid} --duration {self.duration}...")
        cmd = ["syscount-bpfcc", "-p", str(pid), "--duration", str(self.duration)]
        try:
            r = subprocess.run(cmd, capture_output=True, text=True,
                               timeout=self.duration + 10)
            return self._parse_syscount(r.stdout + r.stderr)
        except (subprocess.TimeoutExpired, FileNotFoundError) as e:
            print(f"  syscount failed: {e}")
            return {}

    def _parse_syscount(self, output: str) -> dict:
        """Parse syscount text output into dict."""
        import re
        counts = {}
        for line in output.splitlines():
            # Format: "sched_yield    5635118   9411457.451"
            m = re.match(r'^\s*(\w+)\s+(\d+)\s+([\d.]+)', line)
            if m:
                counts[m.group(1)] = {
                    "count": int(m.group(2)),
                    "time_us": float(m.group(3))
                }
        return counts

    def analyze(self, syscounts: dict) -> ObservabilityResult:
        """Interpret syscall counts and produce a verdict."""
        yield_count  = syscounts.get("sched_yield", {}).get("count", 0)
        futex_count  = syscounts.get("futex",       {}).get("count", 0)
        mmap_count   = syscounts.get("mmap",        {}).get("count", 0)

        thrash_ratio = yield_count / (futex_count + 1)
        yield_per_sec = yield_count / self.duration

        if thrash_ratio > self.THRASH_THRESHOLD_HIGH:
            verdict = "THRASHING"
            rec = (
                "OpenBLAS thread pool is thrashing under QEMU. "
                "Rebuild with USE_THREAD=0 NUM_THREADS=1 to eliminate spinning. "
                "This is expected: QEMU cannot parallelize riscv64 threads on x86."
            )
        elif thrash_ratio > self.THRASH_THRESHOLD_MILD:
            verdict = "MILD_SPIN"
            rec = (
                "Moderate thread spinning. Consider USE_THREAD=0 for QEMU validation. "
                "On real riscv64 hardware this will resolve naturally."
            )
        else:
            verdict = "HEALTHY"
            rec = "Thread synchronization looks healthy. Ratio is within normal range."

        return ObservabilityResult(
            binary_name    = "dgemm_validate_riscv",
            duration_sec   = self.duration,
            yield_per_sec  = yield_per_sec,
            futex_count    = futex_count,
            yield_count    = yield_count,
            mmap_count     = mmap_count,
            thrash_ratio   = thrash_ratio,
            thread_verdict = verdict,
            recommendation = rec,
            raw_syscounts  = syscounts,
        )

    def print_bpftrace_scripts(self):
        """Print the bpftrace scripts for manual use."""
        print("\n" + "═"*60)
        print("  BPFTRACE SCRIPTS — run with sudo while qemu is running")
        print("═"*60)

        print("\n[1] Thread-pool thrashing detector (replace PID):")
        print("────────────────────────────────────────────────")
        script = BPFTRACE_SCHED_YIELD.replace("TARGET_PID", "$1")
        print(f"sudo bpftrace -e '{script.strip()}' <PID>")

        print("\n[2] Vector tail overhead proxy:")
        print("────────────────────────────────")
        script = BPFTRACE_VECTOR_PROXY.replace("TARGET_PID", "$1")
        print(f"sudo bpftrace -e '{script.strip()}' <PID>")

        print("\n[3] Memory allocation pattern:")
        print("────────────────────────────────")
        script = BPFTRACE_MEMORY.replace("TARGET_PID", "$1")
        print(f"sudo bpftrace -e '{script.strip()}' <PID>")

        print("\n[4] Quick syscount summary:")
        print("────────────────────────────")
        print("sudo syscount-bpfcc -p <PID> --duration 30")
        print()

    def run(self) -> ObservabilityResult:
        if not self._check_bpftools():
            self.print_bpftrace_scripts()
            # Return stub result
            return ObservabilityResult(
                binary_name   = "dgemm_validate_riscv",
                duration_sec  = self.duration,
                thread_verdict = "STUB",
                recommendation = "Install bpfcc-tools to enable live profiling.",
            )

        pid = self.pid
        if pid is None and self.cmd:
            print(f"  Launching: {self.cmd}")
            proc = subprocess.Popen(self.cmd.split())
            pid = proc.pid
            time.sleep(1)  # let it start up

        if pid is None:
            print("  No PID or command provided.")
            return ObservabilityResult(binary_name="unknown", duration_sec=0)

        syscounts = self.run_syscount(pid)
        result    = self.analyze(syscounts)

        self.print_bpftrace_scripts()
        return result


def main():
    parser = argparse.ArgumentParser(description="OpenBLAS riscv64 eBPF Observer")
    parser.add_argument("--pid",      type=int, help="PID of qemu-riscv64-static process")
    parser.add_argument("--cmd",      type=str, help="Command to launch and observe")
    parser.add_argument("--duration", type=int, default=30,
                        help="Observation duration in seconds")
    parser.add_argument("--report",   type=str, default="openblas_ebpf_report.json")
    args = parser.parse_args()

    observer = OpenBLASObserver(
        pid      = args.pid,
        cmd      = args.cmd,
        duration = args.duration,
    )

    print("\n" + "═"*60)
    print("  OpenBLAS riscv64 eBPF Observability Layer")
    print("═"*60)

    result = observer.run()

    # Print summary
    print("\n  VERDICT:", result.thread_verdict)
    print(f"  yield/sec:    {result.yield_per_sec:,.0f}")
    print(f"  thrash_ratio: {result.thrash_ratio:.1f}  (yield/futex)")
    print(f"  Recommendation: {result.recommendation}")

    # Write JSON
    report = asdict(result)
    Path(args.report).write_text(json.dumps(report, indent=2))
    print(f"\n  Report: {args.report}")
    print("═"*60 + "\n")


if __name__ == "__main__":
    main()
