<div align="center">

# RISC-V HPC Portability & Validation Engine

**LFX Mentorship 2026 — Broadening the RISC-V High Precision Code Base**

[![Arch](https://img.shields.io/badge/arch-riscv64-blue?logo=linux)](https://github.com/Vaibhav805/LFX_riscv)
[![Compiler](https://img.shields.io/badge/gcc-13.3.0%20cross-green)](https://github.com/Vaibhav805/LFX_riscv)
[![ARPACK](https://img.shields.io/badge/ARPACK--ng-3.9.1-orange)](https://github.com/opencollab/arpack-ng)
[![Validation](https://img.shields.io/badge/dsbdr*-PASS%208.97e--15-brightgreen)](reports/final_submission_report.json)
[![eBPF](https://img.shields.io/badge/eBPF-syscount%20integrated-purple)](docs/ebpf-observations.md)

**Applicant:** Vaibhav Binwal · IIT Jodhpur | **Mentor:** Kurt Keville (MIT)

</div>

---

## What This Repo Proves

A working, automated, end-to-end pipeline — built before writing a single line of proposal:

| Step | What happens | Evidence |
|------|-------------|---------|
| **1. Self-heal** | Fixes apt sources, sysroot symlinks, file ownership automatically | `pipeline/run_arpack_final.sh` Steps 0–3 |
| **2. Cross-compile** | ARPACK-ng 3.9.1 → riscv64 ELF via GCC 13 | `toolchain/riscv64-toolchain.cmake` |
| **3. Execute** | Runs binaries under `qemu-riscv64-static` | Step 7 in pipeline |
| **4. Extract** | Regex pulls residuals from all 3 ARPACK output formats | Extractor in `pipeline/run_arpack_final.sh` |
| **5. Observe** | eBPF syscall profile via `syscount-bpfcc` | `docs/ebpf-observations.md` |
| **6. Report** | `reports/final_submission_report.json` — per-binary PASS/FAIL | Machine-readable JSON |

**Single command to reproduce:** `sudo bash pipeline/run_arpack_final.sh`

---

## Validated Numerical Results

All errors are 4+ orders of magnitude below the double-precision threshold (1×10⁻¹⁰):

| Binary | Driver Type | Relative Errors | Worst-case | Status |
|--------|------------|----------------|-----------|--------|
| `dsbdr1` | Symmetric band — regular | [6.04e-15, 4.21e-15, 8.97e-15, 3.15e-15] | 8.97e-15 | ✅ PASS |
| `dsbdr2` | Symmetric band — shift-invert | [6.04e-15, 4.21e-15, 8.97e-15, 3.15e-15] | 8.97e-15 | ✅ PASS |
| `dsbdr3` | Symmetric band — Buckling | [6.04e-15, 4.21e-15, 8.97e-15, 3.15e-15] | 8.97e-15 | ✅ PASS |
| `dsbdr4` | Symmetric band — Cayley | [6.04e-15, 4.21e-15, 8.97e-15, 3.15e-15] | 8.97e-15 | ✅ PASS |
| `dsbdr5` | Symmetric band — LAPACK | [6.04e-15, 4.21e-15, 8.97e-15, 3.15e-15] | 8.97e-15 | ✅ PASS |
| `dndrv2` | Non-symmetric — shift-invert | [9.56e-16, 7.78e-16, 1.20e-14] | 1.20e-14 | ✅ PASS |
| `dsdrv3` | Symmetric shift-invert | [6.05e-15, 4.21e-15, 8.97e-15] | 8.97e-15 | ✅ PASS |

**RISC-V produces numerically equivalent results to x86-64.**

---

## Root Causes Diagnosed & Fixed

| Error | Root Cause | Fix |
|-------|-----------|-----|
| `ld: cannot find libc.so.6` | `CMAKE_SYSROOT` overrides cross-gcc's built-in sysroot | Omit `CMAKE_SYSROOT` entirely |
| `E: Unable to locate libblas-dev:riscv64` | `archive.ubuntu.com` returns 404 for ports arches | Use `ports.ubuntu.com` |
| `PermissionError` on report JSON | Previous `sudo` run created root-owned artifacts | `chown -R $SUDO_USER` before Step 1 |
| `Invalid control character at col 180` | `${OUTPUT}` heredoc expansion with newlines in Python | Write to temp file, `open()` to read |
| `dndrv*` showing NO_DATA | ARPACK has 3 different output formats — one regex misses two | Three-pattern extractor in pipeline |

Full details: [docs/toolchain-pitfalls.md](docs/toolchain-pitfalls.md)

---

## Repo Structure

```
LFX_riscv/
├── pipeline/
│   ├── run_arpack_final.sh            # One-command bootstrap + build + audit
│   ├── riscv_validation_engine_v3.py  # Class-based Python validation engine
│   └── make_deb.sh                    # Packages build output as riscv64 .deb
│
├── toolchain/
│   └── riscv64-toolchain.cmake        # Cross-compile config (CMAKE_SYSROOT omitted)
│
├── hal/
│   └── simd.h                         # Portable SIMD: RVV / AVX2+FMA / SSE2 / scalar
│
├── reports/
│   ├── final_submission_report.json   # Per-binary PASS/FAIL results
│   └── arpack-ng_3.9.1_riscv64.deb   # Packaged riscv64 binary
│
├── docs/
│   ├── toolchain-pitfalls.md          # 5 root causes + fixes
│   └── ebpf-observations.md           # syscall profile analysis
│
└── coding-challenge/
    └── hanoi_reccursion.py            # LFX coding challenge solution
```

---

## HAL SIMD Shim — `hal/simd.h`

Zero `#ifdef` in application code. Same call on every architecture:

```c
hal_f64x4 result = hal_fmadd_f64x4(a, b, c);  // a*b + c
double     dot   = hal_dot4(vec_a, vec_b);
```

| Architecture | Backend | Key Intrinsics |
|-------------|---------|---------------|
| `riscv64` + RVV | RISC-V Vector Extension | `vfmacc_vv_f64m4`, `vle64_v_f64m4`, `vfredosum` |
| `x86_64` AVX2+FMA | Advanced Vector Extensions | `_mm256_fmadd_pd`, `_mm256_loadu_pd` |
| `x86_64` SSE2 | Streaming SIMD Extensions 2 | `_mm_mul_pd`, `_mm_add_pd` |
| Any | Scalar C | Plain loops — bit-identical output, any ISA |

---

## eBPF Observability

`syscount-bpfcc` captured during `qemu-riscv64-static` execution:

```
SYSCALL        COUNT        TIME (us)
futex         326,181   5,577,765,731
epoll_wait     50,134   1,931,155,063
sched_yield 5,635,118       9,411,457
```

The 5.6M `sched_yield` calls are QEMU user-space thread multiplexing — not a riscv64 property. This quantitatively motivates Phase 4 hardware validation. See [docs/ebpf-observations.md](docs/ebpf-observations.md).

---

## Quickstart

```bash
# Prerequisites
sudo apt install gcc-13-riscv64-linux-gnu gfortran-13-riscv64-linux-gnu \
                 qemu-user-static cmake bpfcc-tools

# Clone and run
git clone https://github.com/Vaibhav805/LFX_riscv
cd LFX_riscv
sudo bash pipeline/run_arpack_final.sh
# Output: reports/final_submission_report.json
```

---

## Background

**eBPF packet engine — 20M pps:** XDP programs using `BPF_MAP_TYPE_PERCPU_ARRAY` for lock-free per-core counters. Profiled with `bpftool` and `syscount-bpfcc` — same tools used for riscv64 observability here.

**Low-latency order book — sub-100µs:** Lock-free matching engine with `_mm256_cmp_pd` SIMD price comparison. The AVX2 intrinsics abstracted in `hal/simd.h` are from this production context.

---

<div align="center">
IIT Jodhpur · LFX Mentorship 2026 · <a href="https://github.com/Vaibhav805">GitHub</a>
</div>
