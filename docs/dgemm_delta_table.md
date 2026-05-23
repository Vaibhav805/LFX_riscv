# DGEMM Scalar vs Linked OpenBLAS Delta

| Case | Scalar err | Linked err | Delta | Both |
|---|---:|---:|---:|---|
| tiny_1x1 | 0.0000e+00 | 0.0000e+00 | 0.0000e+00 | PASS |
| tiny_2x2 | 0.0000e+00 | 0.0000e+00 | 0.0000e+00 | PASS |
| tiny_3x3 | 0.0000e+00 | 0.0000e+00 | 0.0000e+00 | PASS |
| tiny_4x4 | 0.0000e+00 | 0.0000e+00 | 0.0000e+00 | PASS |
| pow2_8 | 0.0000e+00 | 0.0000e+00 | 0.0000e+00 | PASS |
| pow2_16 | 0.0000e+00 | 0.0000e+00 | 0.0000e+00 | PASS |
| pow2_32 | 0.0000e+00 | 0.0000e+00 | 0.0000e+00 | PASS |
| pow2_64 | 0.0000e+00 | 0.0000e+00 | 0.0000e+00 | PASS |
| pow2_128 | 3.6702e-16 | 3.6702e-16 | 0.0000e+00 | PASS |
| pow2_256 | 5.1100e-16 | 5.1100e-16 | 0.0000e+00 | PASS |
| pow2_512 | 7.8616e-16 | 7.8616e-16 | 0.0000e+00 | PASS |
| pow2_1024 | 1.1158e-15 | 1.1158e-15 | 0.0000e+00 | PASS |
| npo2_37 | 0.0000e+00 | 0.0000e+00 | 0.0000e+00 | PASS |
| npo2_41x53 | 0.0000e+00 | 0.0000e+00 | 0.0000e+00 | PASS |
| npo2_99 | 0.0000e+00 | 0.0000e+00 | 0.0000e+00 | PASS |
| npo2_127 | 3.6321e-16 | 3.6321e-16 | 0.0000e+00 | PASS |
| npo2_255 | 5.1063e-16 | 5.1063e-16 | 0.0000e+00 | PASS |
| npo2_333 | 6.1846e-16 | 6.1846e-16 | 0.0000e+00 | PASS |
| npo2_500 | 7.7652e-16 | 7.7652e-16 | 0.0000e+00 | PASS |
| npo2_777 | 9.7182e-16 | 9.7182e-16 | 0.0000e+00 | PASS |
| npo2_999 | 1.1007e-15 | 1.1007e-15 | 0.0000e+00 | PASS |
| rect_1x1000 | 1.1014e-15 | 1.1014e-15 | 0.0000e+00 | PASS |
| rect_1000x1 | 1.0396e-15 | 1.0396e-15 | 0.0000e+00 | PASS |
| rect_tall | 5.0900e-16 | 5.0900e-16 | 0.0000e+00 | PASS |
| rect_wide | 5.0675e-16 | 5.0675e-16 | 0.0000e+00 | PASS |
| acc_K2048 | 1.5898e-15 | 1.5898e-15 | 0.0000e+00 | PASS |
| acc_K4096 | 2.1664e-15 | 2.1664e-15 | 0.0000e+00 | PASS |
| ab_zero_beta | 0.0000e+00 | 0.0000e+00 | 0.0000e+00 | PASS |
| ab_one_one | 0.0000e+00 | 0.0000e+00 | 0.0000e+00 | PASS |
| ab_neg_alpha | 0.0000e+00 | 0.0000e+00 | 0.0000e+00 | PASS |
| ab_pi_alpha | 0.0000e+00 | 0.0000e+00 | 0.0000e+00 | PASS |
| ab_neg_beta | 0.0000e+00 | 0.0000e+00 | 0.0000e+00 | PASS |
| ab_half | 0.0000e+00 | 0.0000e+00 | 0.0000e+00 | PASS |
| ab_both_neg | 0.0000e+00 | 0.0000e+00 | 0.0000e+00 | PASS |
| transNN_64 | 0.0000e+00 | 0.0000e+00 | 0.0000e+00 | PASS |
| transTN_64 | 0.0000e+00 | 0.0000e+00 | 0.0000e+00 | PASS |
| transNT_64 | 0.0000e+00 | 0.0000e+00 | 0.0000e+00 | PASS |
| transTT_64 | 0.0000e+00 | 0.0000e+00 | 0.0000e+00 | PASS |
| transTN_255 | 5.1158e-16 | 5.1158e-16 | 0.0000e+00 | PASS |
| transNT_333 | 6.1661e-16 | 6.1661e-16 | 0.0000e+00 | PASS |
| transNN_255 | 5.1105e-16 | 5.1105e-16 | 0.0000e+00 | PASS |
| transTT_333 | 6.1864e-16 | 6.1864e-16 | 0.0000e+00 | PASS |

Compared cases: 42
Maximum absolute relative-error delta: 0.0000e+00

Note: the RVV-linked archive is still branded `RISCV64_GENERIC`, but the rebuilt
archive contains 724 matching RVV opcodes after mapping the riscv64 vector
kernels. DGEMM itself shows identical numerical behavior in this validation
because the patched mappings cover Level 1 and Level 2 kernels rather than the
DGEMM microkernel.
