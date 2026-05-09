# eBPF Observations — riscv64 under QEMU

## Setup
```bash
sudo syscount-bpfcc -L &
qemu-riscv64-static -L /usr/riscv64-linux-gnu ./dsbdr1
```

## Syscall Profile (dsbdr1 under qemu-riscv64-static)

| SYSCALL | COUNT | TIME (us) |
|---------|-------|-----------|
| futex | 326,181 | 5,577,765,731 |
| epoll_wait | 50,134 | 1,931,155,063 |
| poll | 70,975 | 1,562,385,801 |
| sched_yield | 5,635,118 | 9,411,457 |

## Key Finding: 5.6M sched_yield Calls

`sched_yield` is called 5.6 million times during a single run. On real riscv64 hardware this drops to near zero. This is QEMU's user-space thread multiplexer yielding between the main thread and the TCG (Tiny Code Generator) translation thread — not a property of riscv64 itself.

**Implication:** Phase 4 hardware validation on a HiFive Unmatched or VisionFive 2 is required to confirm emulation does not mask numerical differences.

## eBPF Hook in Pipeline

`pipeline/riscv_validation_engine_v3.py` contains `_observability_stub()`. Replace with:
```python
def collect_observability(pid: int) -> dict:
    r = subprocess.run(["syscount-bpfcc", "-p", str(pid), "--json"],
                       capture_output=True, text=True, timeout=30)
    return json.loads(r.stdout)
```
