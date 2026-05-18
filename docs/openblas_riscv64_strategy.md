# OpenBLAS riscv64 Cross-Compilation & Validation Strategy
## Triple-A (Automated · Architectural · Application-Ready) for LFX Mentorship

**Dependency chain:** OpenBLAS → ARPACK-ng → ~80 eigenvalue codes  
**Host:** x86_64 Ubuntu 24.04, GCC 13.3.0  
**Target:** riscv64, `-march=rv64gcv -mabi=lp64d` (RVV 1.0)

---

## 1. Build Logic — Solving the `getarch` Host-Execution Error

### Root Cause

When you run a naive `make CC=riscv64-linux-gnu-gcc-13`, the OpenBLAS Makefile does this internally:

```
Phase 1: compile getarch.c  --[CC=riscv64-gcc]--> getarch  (riscv64 ELF)
         run ./getarch                             ← FAILS on x86_64 host
         (getarch detects CPU caps at runtime to select kernel variants)

Phase 2: compile BLAS kernels  --[CC=riscv64-gcc]--> *.o (riscv64)
```

The failure is `./getarch: cannot execute binary file: Exec format error`. Every tutorial that says "just set CC=" misses that getarch must **run** on the build host.

### The Fix: Two Separate Variables + TARGET Bypass

```bash
make -j$(nproc)                              \
    TARGET=RISCV64_GENERIC                   \  # ← CRITICAL: bypasses getarch
    CROSS=1                                  \  # ← tells Makefile this is cross-build
    CROSS_SUFFIX=riscv64-linux-gnu-          \  # ← prefix for all cross tools
    CC=riscv64-linux-gnu-gcc-13              \  # ← compiles BLAS .c kernel files
    FC=riscv64-linux-gnu-gfortran-13         \  # ← compiles BLAS .f kernel files
    HOSTCC=gcc                               \  # ← compiles getarch (x86 native!)
    AR=riscv64-linux-gnu-ar                  \  # ← archives riscv64 .o files
    RANLIB=riscv64-linux-gnu-ranlib          \  # ← indexes the archive
    BINARY=64                                \  # ← 64-bit ABI
    NO_SHARED=1                              \  # ← static only (avoids rpath issues)
    USE_THREAD=0                             \  # ← disable thread pool (QEMU pitfall)
    NUM_THREADS=1                            \  # ← single-threaded for QEMU
    CFLAGS="-march=rv64gcv -mabi=lp64d -O2" \  # ← RVV 1.0 enabled
    FFLAGS="-march=rv64gcv -mabi=lp64d -O2"    # ← Fortran RVV enabled
```

**Why `TARGET=RISCV64_GENERIC` works:** OpenBLAS's `TargetList.txt` defines a pre-built kernel configuration for each target. When `TARGET=` is set, the Makefile skips getarch entirely and uses that configuration directly. `RISCV64_GENERIC` selects the riscv64 kernel set including the RVV-accelerated routines.

**Why `HOSTCC=gcc` is separate from `CC=`:** `HOSTCC` compiles build-system utilities (getarch, f_check, c_check) that must run on the x86 host. `CC` compiles the actual BLAS kernel code that runs on riscv64. They must never be swapped.

### libgfortran Dependency — The Hidden Pitfall

OpenBLAS contains Fortran BLAS routines (LAPACK-subset, some BLAS Level 3). When you link a C program against `libopenblas.a`, you **must** also link `libgfortran`:

```bash
# Wrong — linker error: undefined reference to `_gfortran_transfer_real128_write'
riscv64-linux-gnu-gcc-13 test.c -lopenblas -lm

# Correct
riscv64-linux-gnu-gcc-13 test.c \
    -L/usr/lib/gcc-cross/riscv64-linux-gnu/13 \
    -lopenblas \
    -lgfortran \
    -lm
```

The riscv64 `libgfortran.a` is at a non-standard path:
```
/usr/lib/gcc-cross/riscv64-linux-gnu/13/libgfortran.a    ← static
/usr/lib/riscv64-linux-gnu/libgfortran.so.5              ← dynamic
```

For QEMU portability, **always use static linking** (`-static -lgfortran`). Dynamic linking requires the `.so` to exist inside the qemu `-L` prefix at runtime.

---

## 2. RVV Optimization Strategy

### The `-march` String

```bash
# Minimum for RVV 1.0 (GCC 13+)
-march=rv64gcv -mabi=lp64d

# What each extension means:
# rv64  = 64-bit base integer ISA
# g     = shorthand for IMAFD (Integer + Multiply + Atomic + Float32 + Float64)
# c     = Compressed instructions (16-bit encoding, reduces code size)
# v     = Vector standard extension = RVV 1.0 in GCC 13
#
# Optional additions for better RVV codegen:
-march=rv64gcv_zba_zbb_zbc_zbs   # Bit-manipulation (improves vector index ops)
-march=rv64gcv_zvl256b            # Hint: prefer 256-bit vector length
```

### How OpenBLAS Selects RVV Kernels

With `TARGET=RISCV64_GENERIC` and `-march=rv64gcv`, OpenBLAS selects these kernels:

| Operation | Kernel File | RVV Intrinsics Used |
|-----------|------------|---------------------|
| `dgemm` | `kernel/riscv64/dgemm_kernel_4x4_rvv.c` | `vle64_v_f64m4`, `vfmacc_vv_f64m4`, `vse64_v_f64m4` |
| `daxpy` | `kernel/riscv64/daxpy_vector.c` | `vle64_v_f64m8`, `vfmadd_vf_f64m8` |
| `ddot`  | `kernel/riscv64/ddot_vector.c`  | `vle64_v_f64m4`, `vfmul_vv_f64m4`, `vfredosum` |
| `dcopy` | `kernel/riscv64/dcopy_vector.c` | `vle64_v_f64m8`, `vse64_v_f64m8` |
| `dscal` | `kernel/riscv64/dscal_vector.c` | `vle64_v_f64m8`, `vfmul_vf_f64m8` |

### Verifying RVV Instructions Are Emitted

```bash
# After build, disassemble and grep for vector instructions
riscv64-linux-gnu-objdump -d libopenblas.a | grep -E "vle64|vfmacc|vsetvli|vfredosum" | head -20

# Expected output (confirms RVV active):
#    12c:  5509e057  vle64.v  v0,(s3)
#    140:  a20660d7  vfmacc.vv  v0,v0,v0
#    154:  c0327057  vsetvli  zero,t0,e64,m4,ta,ma

# If output is empty: RVV kernels not selected
# Debug: check which kernel was actually compiled
grep -r "RISCV64_GENERIC" Makefile.system  # shows which .c files are selected
```

### VLEN and `vsetvli` — The "Vector Tail" Concept

The RVV `vsetvli` instruction sets the active vector length (`vl`) to `min(avl, VLEN/SEW)`. Under QEMU, the emulated VLEN is typically 128 bits. On real hardware (HiFive Unmatched) it may be 128 or 256 bits.

For `dgemm` with `m4` (LMUL=4), effective width = `4 × VLEN / 64` doubles per operation:
- QEMU VLEN=128: processes **8 doubles per vl iteration**
- Real hw VLEN=256: processes **16 doubles per vl iteration**

This is why Phase 4 hardware validation matters: the "Vector Tail" (the `N % vl` remainder handled by scalar fallback) is 2× larger under QEMU than on real hardware. The numerical result is identical, but the performance profile differs.

---

## 3. eBPF Observability Layer

### Problem 1: Thread-Pool Thrashing Under QEMU

OpenBLAS's default build uses a thread pool (`USE_THREAD=1`). Under QEMU, riscv64 threads cannot actually run in parallel — they serialize through the TCG (Tiny Code Generator) translator. The thread pool spins in a busy-wait loop, calling `sched_yield` millions of times.

**Detection with bpftrace:**

```bash
# Attach to running qemu-riscv64-static process
PID=$(pgrep qemu-riscv64-sta)

sudo bpftrace -e '
tracepoint:syscalls:sys_enter_sched_yield /pid == '"$PID"'/
{
    @yield_count = count();
}
interval:s:1
{
    printf("sched_yield/sec = %lld\n", @yield_count);
    // > 50,000/sec: thread pool thrashing
    // < 1,000/sec:  healthy or USE_THREAD=0
    clear(@yield_count);
}
'
```

**Threshold:**
- `> 50,000 sched_yield/sec` → thrashing, rebuild with `USE_THREAD=0`
- `< 1,000 sched_yield/sec` → healthy

**Fix:** Rebuild OpenBLAS with `USE_THREAD=0 NUM_THREADS=1`. This is the correct configuration for QEMU validation. On real hardware, re-enable threading.

---

### Problem 2: Vector Tail Overhead Estimation

RVV `vsetvli` is a riscv64 user-space instruction — invisible to the x86 host kernel, so it cannot be directly traced with perf/bpftrace. Instead, use a **proxy metric**:

```bash
# Proxy: mmap call frequency during dgemm
# OpenBLAS allocates vector register scratch space per thread
# More mmap calls = more vector context switches = more vsetvli overhead
sudo bpftrace -e '
tracepoint:syscalls:sys_enter_mmap /pid == '"$PID"'/
{
    @mmap_sizes = hist(args->len);
    @mmap_count = count();
}
tracepoint:syscalls:sys_enter_futex /pid == '"$PID"'/
{ @futex = count(); }
tracepoint:syscalls:sys_enter_sched_yield /pid == '"$PID"'/
{ @yield = count(); }
interval:s:5
{
    printf("thrash_ratio (yield/futex) = %.1f\n",
           (float)@yield / (@futex + 1));
    printf("mmap/5sec = %lld\n", @mmap_count);
    clear(@mmap_count); clear(@futex); clear(@yield);
}
'
```

**Interpretation:**
| `yield/futex` ratio | Meaning |
|---------------------|---------|
| < 1.0 | Healthy — threads blocking, not spinning |
| 1–10 | Mild spin — acceptable under QEMU |
| 10–100 | Moderate thrashing |
| > 100 | Severe thrashing — rebuild with `USE_THREAD=0` |

---

### Problem 3: Full syscount Profile

```bash
# Complete syscall audit for dgemm_validate run
sudo syscount-bpfcc -p $PID --duration 30

# Expected healthy profile (USE_THREAD=0):
# SYSCALL      COUNT    TIME(us)
# futex        ~100    ~50,000       ← minimal, just OpenBLAS init
# mmap         ~20     ~10,000       ← panel allocation
# sched_yield  ~50     ~100          ← near zero (thread pool disabled)
# read         ~5      ~500
#
# Unhealthy profile (USE_THREAD=1 under QEMU):
# sched_yield  5,635,118  9,411,457  ← massive spin
```

---

## 4. Numerical Validation — `cblas_dgemm` Golden Reference

### Strategy

The validation compares two implementations:

```
Golden Reference:  naive O(n³) triple-loop in C
                   no SIMD, no reordering, no approximation
                   → this is the mathematical ground truth
                           ↓
cblas_dgemm:       OpenBLAS RVV-accelerated kernel
                   packed panels, register blocking, vfmacc pipeline
                           ↓
Relative Error:    ||C_ref - C_blas||_F / ||C_ref||_F  ≤ 1e-10
```

### Test Matrix

The test covers 13 cases targeting known numerical edge cases:

| Test | Dimensions | Purpose |
|------|-----------|---------|
| `square_small` | 4×4×4 | Base case — no vectorization |
| `square_medium` | 64×64×64 | Core BLAS path |
| `square_large` | 256×256×256 | L2 cache boundary |
| `square_xl` | 512×512×512 | Panel packing stress |
| `rect_tall` | 256×64×128 | Non-square M>>N |
| `rect_wide` | 64×256×128 | Non-square N>>M |
| `alpha_beta` | 64×64×64 | α=2.5, β=0.5 — non-trivial accumulate |
| `transA/B/AB` | 64×64×64 | Transpose paths |
| `skinny_K` | 128×128×4 | K much smaller than M,N |
| `fat_K` | 4×4×512 | K much larger — accumulation error |
| `non_power2` | 37×41×53 | Unaligned dimensions — vector tail |

The `non_power2` case (37×41×53) is the most important for RVV validation — these dimensions don't align to `vl` boundaries, so every BLAS kernel must handle a "vector tail" using a scalar remainder loop. If the tail is implemented incorrectly, this case fails while power-of-2 cases pass.

### Build Commands

```bash
# Cross-compile for riscv64
riscv64-linux-gnu-gcc-13 \
    -march=rv64gcv -mabi=lp64d -O2 \
    -I${INSTALL_DIR}/include \
    -o dgemm_validate_riscv \
    dgemm_validate.c \
    -L${INSTALL_DIR}/lib \
    -L/usr/lib/gcc-cross/riscv64-linux-gnu/13 \
    -lopenblas -lgfortran -lm -static

# Run under QEMU
qemu-riscv64-static -L /usr/riscv64-linux-gnu ./dgemm_validate_riscv dgemm_report.json

# Build x86 version for direct comparison
gcc -O2 -o dgemm_validate_x86 dgemm_validate.c -lopenblas -lm
./dgemm_validate_x86 dgemm_report_x86.json
```

### Expected Output

```
╔══════════════════════════════════════════════════════════════╗
║  cblas_dgemm Numerical Validation — riscv64 vs Golden Ref   ║
╚══════════════════════════════════════════════════════════════╝

  square_small          4x  4x  4  rel_err=0.0000e+00  max_err=0.0000e+00  [PASS]
  square_medium        64x 64x 64  rel_err=1.2341e-15  max_err=4.4409e-16  [PASS]
  square_large        256x256x256  rel_err=8.9732e-15  max_err=1.7764e-15  [PASS]
  square_xl           512x512x512  rel_err=9.1204e-15  max_err=3.5527e-15  [PASS]
  non_power2           37x 41x 53  rel_err=6.2341e-15  max_err=8.8818e-16  [PASS]
  ...

  PASS: 13  FAIL: 0
  Worst relative error: 9.12e-15  (square_xl)
  Threshold: 1e-10
  Status: ALL PASS
```

### Cross-Architecture Comparison

To compare riscv64 output directly against x86 output:

```python
import json, numpy as np

def compare_reports(riscv_json, x86_json):
    """
    Load two dgemm_report.json files and compare per-case errors.
    Confirms riscv64 and x86 agree to within floating-point rounding.
    """
    riscv = {r["case"]: r for r in json.load(open(riscv_json))["results"]}
    x86   = {r["case"]: r for r in json.load(open(x86_json ))["results"]}

    print(f"{'Case':<20}  {'riscv64 err':>12}  {'x86 err':>12}  {'delta':>12}")
    for case in riscv:
        r_err = riscv[case]["relative_error"]
        x_err = x86[case]["relative_error"]
        delta = abs(r_err - x_err)
        status = "✅" if delta < 1e-12 else "⚠️"
        print(f"{case:<20}  {r_err:12.4e}  {x_err:12.4e}  {delta:12.4e}  {status}")
```

---

## 5. Complete Command Sequence

```bash
# 1. Clone and build
git clone --depth=1 --branch v0.3.26 https://github.com/OpenMathLib/OpenBLAS.git
bash build_openblas_riscv64.sh OpenBLAS

# 2. Validate numerically
qemu-riscv64-static -L /usr/riscv64-linux-gnu ./dgemm_validate_riscv dgemm_report.json

# 3. Observe eBPF profile
PID=$(pgrep qemu-riscv64-sta)
sudo syscount-bpfcc -p $PID --duration 30
# or
sudo python3 openblas_ebpf_observe.py --pid $PID --duration 30

# 4. Package
dpkg-deb --info openblas_0.3.26_riscv64.deb

# 5. Add to audit pipeline
# In run_arpack_final.sh BINARIES array:
BINARIES=(dsbdr1 dsbdr2 ... dgemm_validate_riscv)
```

---

## 6. Key Pitfalls Summary

| Pitfall | Symptom | Fix |
|---------|---------|-----|
| `getarch` host exec | `Exec format error` | `TARGET=RISCV64_GENERIC` + `HOSTCC=gcc` |
| Missing `libgfortran` | `undefined reference to _gfortran_*` | `-L/usr/lib/gcc-cross/riscv64-linux-gnu/13 -lgfortran` |
| Thread pool thrashing | 5M+ `sched_yield/sec` | `USE_THREAD=0 NUM_THREADS=1` |
| No RVV in binary | `objdump` shows no `vle64` | Confirm `-march=rv64gcv` in CFLAGS |
| Vector tail failures | `non_power2` fails, others pass | Check remainder loop in `dgemm_kernel_4x4_rvv.c` |
| Wrong sysroot | `libc.so.6 not found` | `CROSS=1`, no `CMAKE_SYSROOT` |
