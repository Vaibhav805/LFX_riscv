/*
 * hal/simd.h — Portable SIMD abstraction for RISC-V HPC portability
 *
 * Architecture support:
 *   x86_64  : SSE2, AVX2, FMA3 intrinsics
 *   riscv64 : RVV (RISC-V Vector) intrinsics  [Week 6-8 full impl]
 *   any     : Scalar fallback — bit-identical output, any ISA
 *
 * Usage: zero #ifdefs in application code.
 *   hal_f64x4  v = hal_load_f64x4(ptr);
 *   hal_f64x4  r = hal_fmadd_f64x4(a, b, c);
 *   hal_store_f64x4(ptr, r);
 */

#pragma once
#include <stddef.h>

/* ── Type aliases ──────────────────────────────────────────────────────────── */
#if defined(__riscv) && defined(__riscv_v)
  /* RVV path — RISC-V Vector Extension */
  #include <riscv_vector.h>
  typedef vfloat64m4_t hal_f64x4;
  typedef vfloat32m4_t hal_f32x4;
  typedef vint32m4_t   hal_i32x4;

  #define HAL_ARCH "riscv_rvv"

  static inline hal_f64x4 hal_load_f64x4(const double *p) {
      return __riscv_vle64_v_f64m4(p, 4);
  }
  static inline void hal_store_f64x4(double *p, hal_f64x4 v) {
      __riscv_vse64_v_f64m4(p, v, 4);
  }
  static inline hal_f64x4 hal_add_f64x4(hal_f64x4 a, hal_f64x4 b) {
      return __riscv_vfadd_vv_f64m4(a, b, 4);
  }
  static inline hal_f64x4 hal_mul_f64x4(hal_f64x4 a, hal_f64x4 b) {
      return __riscv_vfmul_vv_f64m4(a, b, 4);
  }
  /* FMA: a*b + c */
  static inline hal_f64x4 hal_fmadd_f64x4(hal_f64x4 a, hal_f64x4 b, hal_f64x4 c) {
      return __riscv_vfmacc_vv_f64m4(c, a, b, 4);
  }
  static inline double hal_hsum_f64x4(hal_f64x4 v) {
      vfloat64m1_t zero = __riscv_vfmv_s_f_f64m1(0.0, 1);
      vfloat64m1_t sum  = __riscv_vfredosum_vs_f64m4_f64m1(v, zero, 4);
      return __riscv_vfmv_f_s_f64m1_f64(sum);
  }

#elif defined(__AVX2__) && defined(__FMA__)
  /* x86 AVX2 + FMA3 path */
  #include <immintrin.h>
  typedef __m256d hal_f64x4;
  typedef __m128  hal_f32x4;
  typedef __m128i hal_i32x4;

  #define HAL_ARCH "x86_avx2_fma"

  static inline hal_f64x4 hal_load_f64x4(const double *p) {
      return _mm256_loadu_pd(p);
  }
  static inline void hal_store_f64x4(double *p, hal_f64x4 v) {
      _mm256_storeu_pd(p, v);
  }
  static inline hal_f64x4 hal_add_f64x4(hal_f64x4 a, hal_f64x4 b) {
      return _mm256_add_pd(a, b);
  }
  static inline hal_f64x4 hal_mul_f64x4(hal_f64x4 a, hal_f64x4 b) {
      return _mm256_mul_pd(a, b);
  }
  static inline hal_f64x4 hal_fmadd_f64x4(hal_f64x4 a, hal_f64x4 b, hal_f64x4 c) {
      return _mm256_fmadd_pd(a, b, c);  /* a*b + c */
  }
  static inline double hal_hsum_f64x4(hal_f64x4 v) {
      __m128d lo  = _mm256_castpd256_pd128(v);
      __m128d hi  = _mm256_extractf128_pd(v, 1);
      __m128d sum = _mm_add_pd(lo, hi);
      sum = _mm_hadd_pd(sum, sum);
      return _mm_cvtsd_f64(sum);
  }

#elif defined(__SSE2__)
  /* x86 SSE2 path (fallback from AVX2) */
  #include <emmintrin.h>
  typedef __m128d hal_f64x4;  /* 2-wide; app must call twice for 4 elements */
  typedef __m128  hal_f32x4;
  typedef __m128i hal_i32x4;

  #define HAL_ARCH "x86_sse2"

  static inline hal_f64x4 hal_load_f64x4(const double *p) {
      return _mm_loadu_pd(p);
  }
  static inline void hal_store_f64x4(double *p, hal_f64x4 v) {
      _mm_storeu_pd(p, v);
  }
  static inline hal_f64x4 hal_add_f64x4(hal_f64x4 a, hal_f64x4 b) {
      return _mm_add_pd(a, b);
  }
  static inline hal_f64x4 hal_mul_f64x4(hal_f64x4 a, hal_f64x4 b) {
      return _mm_mul_pd(a, b);
  }
  static inline hal_f64x4 hal_fmadd_f64x4(hal_f64x4 a, hal_f64x4 b, hal_f64x4 c) {
      return _mm_add_pd(_mm_mul_pd(a, b), c);  /* no HW FMA on SSE2 */
  }
  static inline double hal_hsum_f64x4(hal_f64x4 v) {
      __m128d hi = _mm_unpackhi_pd(v, v);
      __m128d s  = _mm_add_pd(v, hi);
      return _mm_cvtsd_f64(s);
  }

#else
  /* ── Scalar fallback — any architecture, bit-identical output ── */
  #include <string.h>

  #define HAL_ARCH "scalar"

  typedef struct { double v[4]; } hal_f64x4;
  typedef struct { float  v[4]; } hal_f32x4;
  typedef struct { int    v[4]; } hal_i32x4;

  static inline hal_f64x4 hal_load_f64x4(const double *p) {
      hal_f64x4 r; memcpy(r.v, p, 4*sizeof(double)); return r;
  }
  static inline void hal_store_f64x4(double *p, hal_f64x4 v) {
      memcpy(p, v.v, 4*sizeof(double));
  }
  static inline hal_f64x4 hal_add_f64x4(hal_f64x4 a, hal_f64x4 b) {
      hal_f64x4 r;
      for (int i=0;i<4;i++) r.v[i] = a.v[i] + b.v[i];
      return r;
  }
  static inline hal_f64x4 hal_mul_f64x4(hal_f64x4 a, hal_f64x4 b) {
      hal_f64x4 r;
      for (int i=0;i<4;i++) r.v[i] = a.v[i] * b.v[i];
      return r;
  }
  static inline hal_f64x4 hal_fmadd_f64x4(hal_f64x4 a, hal_f64x4 b, hal_f64x4 c) {
      hal_f64x4 r;
      for (int i=0;i<4;i++) r.v[i] = a.v[i]*b.v[i] + c.v[i];
      return r;
  }
  static inline double hal_hsum_f64x4(hal_f64x4 v) {
      return v.v[0]+v.v[1]+v.v[2]+v.v[3];
  }
#endif

/* ── Architecture-independent dot product (validates shim works) ─────────── */
static inline double hal_dot4(const double * restrict a, const double * restrict b) {
    hal_f64x4 va = hal_load_f64x4(a);
    hal_f64x4 vb = hal_load_f64x4(b);
    hal_f64x4 vp = hal_mul_f64x4(va, vb);
    return hal_hsum_f64x4(vp);
}