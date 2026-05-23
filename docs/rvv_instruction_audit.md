## RVV Instruction Audit

Audit date: 2026-05-23

OpenBLAS was rebuilt from `OpenBLAS/` with `TARGET=RISCV64_GENERIC` and
`-march=rv64gcv -mabi=lp64d` after prepending the riscv64 vector kernel mappings
to `kernel/riscv64/KERNEL`.

### Opcode Count

```bash
riscv64-linux-gnu-objdump -d OpenBLAS/libopenblas_riscv64_generic-r0.3.26.a 2>/dev/null \
  | grep -c "vle64\|vfmacc\|vsetvli\|vlse64\|vfmul\|vfadd\|vfredosum"
```

| Archive | RVV opcode count | Result |
|---|---:|---|
| `OpenBLAS/libopenblas_riscv64_generic-r0.3.26.a` | 724 | RVV present |
| `openblas_scalar_install/lib/libopenblas_scalar.a` | 0 | Scalar no-V baseline |

The scalar reference was audited with:

```bash
riscv64-linux-gnu-objdump -d openblas_scalar_install/lib/libopenblas_scalar.a 2>/dev/null \
  | grep -c "vle64\|vfmacc\|vsetvli"
```

Observed scalar count: `0`.

### Kernel Symbols Containing RVV Instructions

The rebuilt `RISCV64_GENERIC` archive contains RVV instructions in the expected
Level 1 and Level 2 kernel symbols:

| Kernel symbol | RVV mnemonics observed |
|---|---|
| `dasum_k` | `vlse64`, `vsetvli` |
| `daxpby_k` | `vsetvli` |
| `daxpy_k` | `vsetvli` |
| `dcopy_k` | `vsetvli` |
| `ddot_k` | `vsetvli` |
| `dgemv_n` | `vsetvli` |
| `dgemv_t` | `vsetvli` |
| `dnrm2_k` | `vsetvli` |
| `dscal_k` | `vsetvli` |
| `idamax_k` | `vsetvli` |
| `idamin_k` | `vsetvli` |
| `isamax_k` | `vsetvli` |
| `isamin_k` | `vsetvli` |
| `sasum_k` | `vsetvli` |
| `saxpby_k` | `vsetvli` |
| `saxpy_k` | `vsetvli` |
| `scopy_k` | `vsetvli` |
| `sdot_k` | `vsetvli` |
| `sgemv_n` | `vsetvli` |
| `sgemv_t` | `vsetvli` |
| `snrm2_k` | `vsetvli` |
| `sscal_k` | `vsetvli` |

Internal objdump labels in those kernels also contain `vle64`, `vlse64`,
`vfmacc`, `vfmul`, `vfadd`, and `vfredosum`, accounting for the full 724-opcode
audit count.

### Validation Summary

| Validation | Result | Worst relative error |
|---|---:|---:|
| DGEMM scalar reference | 42/42 PASS | `2.1664e-15` |
| DGEMM RVV-linked build | 42/42 PASS | `2.1664e-15` |
| BLAS L1/L2 RVV-linked build | 11/11 PASS | `1.1236e-15` |
| Hilbert condition stress | 5/5 PASS | `1.3634e-16` |
| LAPACK validation | 2/2 PASS | `1.9984e-15` |

DGEMM scalar-vs-RVV max relative-error delta: `0.0000e+00`.

Reproducibility: `tests/dgemm_rvv` produced the same hash across 10 independent
QEMU runs: `cdf6295e231338f0f8fa86ee410e3f62`.
