## Hardware Speedup Prediction Matrix

The table is designed to be filled from `docs/ebpf_*_profile.txt` outputs. Use
`scripts/openblas_ebpf_observe.py` or the planned profile parser to replace
`TBD` entries with measured syscall counts.

| Code | QEMU futex | QEMU sched_yield | Predicted hardware speedup | Basis |
|---|---:|---:|---:|---|
| OpenBLAS dgemm scalar | TBD | TBD | 2-3x | scalar compute plus QEMU syscall overhead |
| OpenBLAS dgemm RVV | TBD | TBD | 5-10x | vector execution on hardware, QEMU cannot model RVV throughput |
| ARPACK-ng dsbdr1 | TBD | TBD | 2-5x | scheduler/yield overhead dominates under emulation |
| BLAS full L1/L2 RVV | TBD | TBD | 3-6x | memory bandwidth plus vectorized inner loops |
| LAPACK dgels/dgesv | TBD | TBD | ~3x | dense factorization with mixed BLAS/LAPACK compute |
| SPOOLES | TBD | TBD | ~4x | sparse memory access pattern and legacy C scalar overhead |

## Scalar vs RVV Kernel-Footprint Check

The scalar and RVV DGEMM binaries should be profiled from the same validator and
case set. If the futex, mmap, or sched_yield counts diverge, that indicates a
different internal OpenBLAS route rather than input variation.
