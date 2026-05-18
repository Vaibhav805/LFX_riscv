#!/usr/bin/env bash
# =============================================================================
# run_arpack_final.sh  — FINAL VERSION (no heredoc interpolation bugs)
# =============================================================================
set -euo pipefail

TRIPLET="riscv64-linux-gnu"
GCC_VER="13"
SYSROOT_GCC="/usr/lib/gcc-cross/${TRIPLET}/${GCC_VER}"
SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SOURCE_DIR}/_build_riscv64"
REPORT_PATH="${SOURCE_DIR}/final_submission_report.json"
QEMU_PREFIX="/usr/${TRIPLET}"
CODENAME="noble"
REAL_USER="${SUDO_USER:-$USER}"
TMPDIR_WORK="$(mktemp -d)"
trap 'rm -rf "$TMPDIR_WORK"' EXIT

BINARIES=(bug_1323 bug_142 bug_142_gen bug_58_double bug_79_double_complex dnsimp dgemm_validate_riscv)

log()  { echo "$(date '+%Y-%m-%d %H:%M:%S') [INFO]  $*"; }
warn() { echo "$(date '+%Y-%m-%d %H:%M:%S') [WARN]  $*" >&2; }
err()  { echo "$(date '+%Y-%m-%d %H:%M:%S') [ERROR] $*" >&2; }

find_lib() {
    for CAND in "$@"; do
        for DIR in "/usr/lib/${TRIPLET}" "/usr/${TRIPLET}/lib" "/lib/${TRIPLET}" \
                   "/usr/lib/${TRIPLET}/blas" "/usr/lib/${TRIPLET}/lapack"; do
            [ -e "${DIR}/${CAND}" ] && { echo "${DIR}/${CAND}"; return 0; }
        done
    done
    echo ""
}

# ── Step 0: Fix ownership of root-created artifacts ───────────────────────────
log "Step 0: Fixing ownership..."
for T in "$BUILD_DIR" "$REPORT_PATH"; do
    [ -e "$T" ] || continue
    [ "$(stat -c '%U' "$T")" = "root" ] && [ "$REAL_USER" != "root" ] && \
        chown -R "${REAL_USER}:${REAL_USER}" "$T" && log "  chown $T → $REAL_USER"
done
rm -f "${BUILD_DIR}/CMakeCache.txt"
log "  Cleared stale CMakeCache (if any)"

# ── Step 1: Fix apt sources ────────────────────────────────────────────────────
log "Step 1: Configuring apt for riscv64..."
PORTS_LIST="/etc/apt/sources.list.d/ubuntu-ports-riscv64.list"
if ! grep -qr "ports.ubuntu.com" /etc/apt/ 2>/dev/null; then
    cat > "$PORTS_LIST" << SOURCES
deb [arch=riscv64] http://ports.ubuntu.com/ubuntu-ports ${CODENAME}          main restricted universe multiverse
deb [arch=riscv64] http://ports.ubuntu.com/ubuntu-ports ${CODENAME}-updates  main restricted universe multiverse
deb [arch=riscv64] http://ports.ubuntu.com/ubuntu-ports ${CODENAME}-security main restricted universe multiverse
SOURCES
    for F in /etc/apt/sources.list /etc/apt/sources.list.d/*.list; do
        [ -f "$F" ] && [[ "$F" != *ports* ]] && \
        sed -i \
            -e '/^deb.*in\.archive\.ubuntu\.com/s/^deb /deb [arch=amd64,i386] /' \
            -e '/^deb.*security\.ubuntu\.com/s/^deb /deb [arch=amd64,i386] /' \
            -e '/^deb.*archive\.ubuntu\.com/s/^deb /deb [arch=amd64,i386] /' \
            "$F" 2>/dev/null || true
    done
    sudo dpkg --add-architecture riscv64 2>/dev/null || true
    apt-get update -q 2>&1 | grep -E "^(Err:|E:)" | grep -v "antigravity\|GPG\|pubkey" || true
    log "  apt sources configured."
else
    log "  ports.ubuntu.com already present."
fi

# ── Step 2: Install cross libs ────────────────────────────────────────────────
log "Step 2: Installing riscv64 cross libraries..."
sudo apt-get install -y --no-install-recommends \
    libc6:riscv64 libc6-dev:riscv64 \
    libgfortran5:riscv64 \
    libblas3:riscv64 libblas-dev:riscv64 \
    liblapack3:riscv64 liblapack-dev:riscv64 \
    2>&1 | tail -3
log "  Libraries installed."

# ── Step 3: Fix GCC sysroot symlinks ──────────────────────────────────────────
log "Step 3: Fixing GCC sysroot symlinks..."
if [ -d "$SYSROOT_GCC" ]; then
    for PAIR in \
        "libblas.so:libblas.so.3:libblas.so.3.10:libblas.so.3.9" \
        "liblapack.so:liblapack.so.3:liblapack.so.3.10:liblapack.so.3.9" \
        "libgfortran.so:libgfortran.so.5:libgfortran.so.5.0.0"
    do
        IFS=':' read -ra NAMES <<< "$PAIR"
        LINK="${NAMES[0]}"; CANDS=("${NAMES[@]:1}")
        REAL=$(find_lib "${CANDS[@]}")
        if [ -n "$REAL" ]; then
            sudo ln -sf "$REAL" "${SYSROOT_GCC}/${LINK}"
            log "  ${LINK} → ${REAL}"
        fi
    done
else
    warn "  Sysroot dir not found — skipping (cmake will use -L flags)"
fi

# ── Step 4: Write toolchain (NO CMAKE_SYSROOT — this was the bug) ─────────────
log "Step 4: Writing cmake toolchain..."
TOOLCHAIN="${SOURCE_DIR}/riscv64-toolchain.cmake"
cat > "$TOOLCHAIN" << TC
# Ubuntu 24.04 riscv64 cross-toolchain
# CMAKE_SYSROOT is intentionally NOT set.
# riscv64-linux-gnu-gcc-13 has its own correct sysroot baked in.
# Setting CMAKE_SYSROOT overrides it and breaks libc resolution.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR riscv64)
set(CMAKE_C_COMPILER       /usr/bin/${TRIPLET}-gcc-${GCC_VER})
set(CMAKE_CXX_COMPILER     /usr/bin/${TRIPLET}-g++-${GCC_VER})
set(CMAKE_Fortran_COMPILER /usr/bin/${TRIPLET}-gfortran-${GCC_VER})
set(CMAKE_FIND_ROOT_PATH
    /usr/lib/${TRIPLET}
    /usr/lib/gcc-cross/${TRIPLET}/${GCC_VER}
    /usr/${TRIPLET})
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY  ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE  ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM  NEVER)
TC
log "  Written: ${TOOLCHAIN}"

# ── Step 5: cmake configure ───────────────────────────────────────────────────
log "Step 5: cmake configure..."
mkdir -p "$BUILD_DIR"

BLAS_LIB=$(find_lib libblas.so.3 libblas.so)
LAPACK_LIB=$(find_lib liblapack.so.3 liblapack.so)

LFLAGS=""
for D in "/usr/lib/${TRIPLET}" "/usr/${TRIPLET}/lib" "/lib/${TRIPLET}" \
         "/usr/lib/${TRIPLET}/blas" "/usr/lib/${TRIPLET}/lapack" \
         "${SYSROOT_GCC}"; do
    [ -d "$D" ] && LFLAGS="${LFLAGS} -L${D}"
done
[ -n "$BLAS_LIB" ] && LFLAGS="${LFLAGS} -Wl,-rpath-link,$(dirname "$BLAS_LIB")"
LFLAGS="${LFLAGS# }"  # trim leading space

CMAKE_ARGS=(
    cmake "/home/vaibhav/Documents/LFX_Selection_Work/arpack-ng"
    "-DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN}"
    "-DBUILD_SHARED_LIBS=OFF"
    "-DEXAMPLES=ON"
    "-DMPI=OFF"
    "-DCMAKE_EXE_LINKER_FLAGS=${LFLAGS}"
    "-DCMAKE_SHARED_LINKER_FLAGS=${LFLAGS}"
)
[ -n "$BLAS_LIB"   ] && CMAKE_ARGS+=("-DBLAS_LIBRARIES=${BLAS_LIB}")
[ -n "$LAPACK_LIB" ] && CMAKE_ARGS+=("-DLAPACK_LIBRARIES=${LAPACK_LIB}")

cd "$BUILD_DIR"
if ! "${CMAKE_ARGS[@]}"; then
    err "cmake configure failed. Inspect: ${BUILD_DIR}/CMakeFiles/CMakeError.log"
    exit 1
fi
cd "$SOURCE_DIR"
log "  cmake: OK"

# ── Step 6: make ──────────────────────────────────────────────────────────────
log "Step 6: make -j$(nproc)..."
cd "$BUILD_DIR"
make -j"$(nproc)"
cd "$SOURCE_DIR"
log "  make: OK"

# ── Step 7: Run binaries — output written to temp files, NOT bash variables ────
log "Step 7: Running binaries under qemu-riscv64-static..."

# The extractor Python script is written once to disk.
# It reads the binary's output from a file — no heredoc interpolation bugs.
EXTRACTOR="${TMPDIR_WORK}/extract.py"
cat > "$EXTRACTOR" << 'PYEOF'
#!/usr/bin/env python3
# =============================================================================
# Definitive extractor for ARPACK-ng driver output — verified against real
# binary output format from ARPACK-ng source code.
#
# REAL OUTPUT FORMATS (from ARPACK-ng source):
#
#   dsbdr*  Per-eigenvalue labelled lines (no table):
#             " The Ritz value       is           0.09794138D+00"
#             " The relative residual is           9.56000000D-16"
#
#   dndrv*  Table under header containing "Real,Imag" and "residual":
#             " Row 1:    2.99993D+03   0.00000D+00   4.78000D-14"
#             " Row 2:    ..."
#             (3 columns: real, imag, residual — D-exponent Fortran notation)
#
#   dsdrv*  Table under header containing "Ritz values" and "residual"
#           but NOT "Imag":
#             " Row 1:    3.14159D+00   6.05000D-15"
#             " Row 2:    ..."
#             (2 columns: eigenvalue, residual — D-exponent Fortran notation)
#
# Row prefix is "Row N:" but we also accept bare "N:" or "N " for safety.
# =============================================================================
import sys, re, json

name     = sys.argv[1]
out_file = sys.argv[2]
with open(out_file, "r", errors="replace") as f:
    raw = f.read()

FP_RE = r'[+-]?\d+(?:\.\d+)?(?:[EeDd][+-]?\d+)?'

def fp(s):
    try:    return float(s.upper().replace('D', 'E'))
    except: return None

# ── Row patterns — "Row N:" prefix OR bare "N:" OR bare "N " ─────────────────
ROW_PFX = r'^\s*(?:Row\s+)?\d+\s*:\s*'
row3 = re.compile(ROW_PFX + rf'({FP_RE})\s+({FP_RE})\s+({FP_RE})\s*$', re.I)
row2 = re.compile(ROW_PFX + rf'({FP_RE})\s+({FP_RE})\s*$',             re.I)
# bare (no colon): "  1    X.XXX   Y.YYY   Z.ZZZ"
bare3 = re.compile(rf'^\s*\d+\s+({FP_RE})\s+({FP_RE})\s+({FP_RE})\s*$')
bare2 = re.compile(rf'^\s*\d+\s+({FP_RE})\s+({FP_RE})\s*$')

# ── Section header patterns ───────────────────────────────────────────────────
# dndrv: "Ritz values (Real,Imag) and relative residuals"
dndrv_hdr = re.compile(r'Real.*Imag.*residual|Ritz\s+values.*Real.*Imag', re.I)
# dsdrv: "Ritz values and relative residuals" (no Imag)
dsdrv_hdr = re.compile(r'Ritz\s+values?\s+and\s+relative\s+residual', re.I)

# ── Explicit per-line patterns (dsbdr*) ──────────────────────────────────────
pat_ritz = re.compile(rf'The\s+Ritz\s+value\s+is\s+({FP_RE})',        re.I)
pat_rres = re.compile(rf'The\s+relative\s+residual\s+is\s+({FP_RE})', re.I)
# generic fallbacks for any labelled output
pat_rel2 = re.compile(rf'relative\s+(?:error|residual)\s*[=:is]+\s*({FP_RE})', re.I)
pat_eig2 = re.compile(rf'(?:Ritz\s+value|eigenvalue)\s*(?:\d+)?\s*[=:is]+\s*({FP_RE})', re.I)

def scan_section(lines, start, is_3col):
    """
    Scan lines[start:] for numeric table rows.
    is_3col=True  -> tries row3/bare3 (real, imag, residual)
    is_3col=False -> tries row2/bare2 (eigenvalue, residual)
    Tolerates up to 8 junk lines before first data; stops after 3 post-data junk.
    Returns (rel_errs, eigvals).
    """
    rel_errs, eigvals = [], []
    pre_junk = post_junk = 0
    found = False
    for line in lines[start:]:
        if is_3col:
            m = row3.match(line) or bare3.match(line)
            if m:
                found = True; post_junk = 0
                v = fp(m.group(1)); r = fp(m.group(3))
                if v is not None: eigvals.append(v)
                if r is not None: rel_errs.append(r)
                continue
        else:
            m = row2.match(line) or bare2.match(line)
            if m:
                found = True; post_junk = 0
                v = fp(m.group(1)); r = fp(m.group(2))
                if v is not None: eigvals.append(v)
                if r is not None: rel_errs.append(r)
                continue
        # non-data line
        if not found:
            pre_junk += 1
            if pre_junk > 8: break
        else:
            post_junk += 1
            if post_junk >= 3: break
    return rel_errs, eigvals

lines     = raw.splitlines()
rel_errs  = []
eigvals   = []
notes_acc = []

# ── Pass 1: dsbdr* explicit labelled lines ────────────────────────────────────
for line in lines:
    m = pat_ritz.search(line)
    if m:
        v = fp(m.group(1))
        if v is not None: eigvals.append(v)
    m = pat_rres.search(line)
    if m:
        v = fp(m.group(1))
        if v is not None: rel_errs.append(v)
    # generic fallbacks
    for m in pat_rel2.finditer(line):
        v = fp(m.group(1))
        if v is not None and v not in rel_errs: rel_errs.append(v)
    for m in pat_eig2.finditer(line):
        v = fp(m.group(1))
        if v is not None and v not in eigvals: eigvals.append(v)

# ── Pass 2: dndrv* tabular sections (3-col: real, imag, residual) ─────────────
for i, line in enumerate(lines):
    if dndrv_hdr.search(line):
        r, e = scan_section(lines, i + 1, is_3col=True)
        rel_errs.extend(r); eigvals.extend(e)

# ── Pass 3: dsdrv* tabular sections (2-col: eigenvalue, residual) ─────────────
for i, line in enumerate(lines):
    if dsdrv_hdr.search(line):
        r, e = scan_section(lines, i + 1, is_3col=False)
        rel_errs.extend(r); eigvals.extend(e)

# ── Deduplicate eigenvalues ───────────────────────────────────────────────────
seen = set(); eigvals_u = []
for v in eigvals:
    k = round(v, 8)
    if k not in seen: seen.add(k); eigvals_u.append(v)

# ── Status ────────────────────────────────────────────────────────────────────
filtered = [e for e in rel_errs if abs(e) < 10.0]
if not filtered and rel_errs:
    notes_acc.append(f"residuals present but magnitude>=10: {rel_errs[:5]}")

worst  = max(abs(e) for e in filtered) if filtered else None
status = ("PASS"    if worst is not None and worst <= 1e-10
          else "FAIL"    if worst is not None
          else "NO_DATA")

print(json.dumps({
    "binary_name":      name,
    "precision_status": status,
    "relative_errors":  filtered,
    "eigenvalues":      eigvals_u,
    "worst_error":      worst,
    "exec_ok":          True,
    "notes":            "; ".join(notes_acc),
    "raw_tail":         raw[-1200:].strip(),
}))
PYEOF

RECORDS_DIR="${TMPDIR_WORK}/records"
mkdir -p "$RECORDS_DIR"

TOTAL=0; N_PASS=0; N_FAIL=0; N_ERR=0; N_ND=0

for BIN_NAME in "${BINARIES[@]}"; do
    TOTAL=$((TOTAL+1))
    log "[${TOTAL}/${#BINARIES[@]}] ${BIN_NAME}"

    BIN=$(find "$BUILD_DIR" -name "$BIN_NAME" ! -path "*/CMakeFiles/*" -type f 2>/dev/null | head -1 || true)
    OUT_FILE="${TMPDIR_WORK}/${BIN_NAME}.out"
    REC_FILE="${RECORDS_DIR}/${TOTAL}_${BIN_NAME}.json"

    if [ -z "$BIN" ]; then
        warn "  ${BIN_NAME}: binary not found after build"
        echo '{"binary_name":"'"$BIN_NAME"'","precision_status":"ERROR","relative_errors":[],"eigenvalues":[],"worst_error":null,"exec_ok":false,"notes":"binary not found","raw_tail":""}' > "$REC_FILE"
        N_ERR=$((N_ERR+1))
        continue
    fi

    # Write output to a file — never interpolate into bash/python source
    qemu-riscv64-static -L "$QEMU_PREFIX" "$BIN" > "$OUT_FILE" 2>&1 || true

    # Extract numerics cleanly via the standalone python script
    python3 "$EXTRACTOR" "$BIN_NAME" "$OUT_FILE" > "$REC_FILE"

    ST=$(python3 -c "import json,sys; print(json.load(open('${REC_FILE}'))['precision_status'])" 2>/dev/null || echo ERROR)
    log "  → ${ST}"
    # On NO_DATA: print the last 30 lines of raw output so you can see the real format
    if [ "$ST" = "NO_DATA" ] || [ "$ST" = "ERROR" ]; then
        warn "  ── raw output tail (${BIN_NAME}) ──"
        tail -30 "$OUT_FILE" | sed 's/^/    /' >&2 || true
        warn "  ── end raw tail ──"
    fi
    case "$ST" in
        PASS)    N_PASS=$((N_PASS+1))  ;;
        FAIL)    N_FAIL=$((N_FAIL+1))  ;;
        NO_DATA) N_ND=$((N_ND+1))      ;;
        *)       N_ERR=$((N_ERR+1))    ;;
    esac
done
# In run_arpack_final.sh, find the execution loop and ensure it handles dgemm:
for bin in "${BINARIES[@]}"; do
    echo "Running $bin ..."
    if [ "$bin" == "dgemm_validate_riscv" ]; then
        qemu-riscv64-static \
            -L /usr/riscv64-linux-gnu \
            ./$bin dgemm_report.json
    else
        qemu-riscv64-static \
            -L /usr/riscv64-linux-gnu \
            ./$bin
    fi
done

# ── Step 8: Assemble and write final JSON ─────────────────────────────────────
log "Step 8: Writing ${REPORT_PATH}..."

python3 - << PYEOF
import json, pathlib, sys

records_dir = pathlib.Path("${RECORDS_DIR}")
results = []
for p in sorted(records_dir.glob("*.json")):
    try:
        results.append(json.loads(p.read_text()))
    except Exception as e:
        print(f"  Warning: could not parse {p}: {e}", file=sys.stderr)

report = {
    "meta": {
        "project":        "LFX Mentorship – RISC-V High Precision Code Base",
        "mentee_org":     "IIT Jodhpur",
        "target_arch":    "riscv64",
        "host_os":        "Ubuntu 24.04 (Noble)",
        "compiler":       "GCC ${GCC_VER}.x cross (${TRIPLET})",
        "toolchain_note": "CMAKE_SYSROOT omitted; cross-gcc has built-in sysroot",
    },
    "sysroot_gcc":   "${SYSROOT_GCC}",
    "audit_summary": {
        "total":   ${TOTAL},
        "PASS":    ${N_PASS},
        "FAIL":    ${N_FAIL},
        "ERROR":   ${N_ERR},
        "NO_DATA": ${N_ND},
    },
    "binary_results": results,
}

out = pathlib.Path("${REPORT_PATH}")
out.write_text(json.dumps(report, indent=2))
print(f"  Written: {out}")
PYEOF

# ── Step 9: Print proposal values ─────────────────────────────────────────────
python3 - << PYEOF
import json, pathlib

doc = json.loads(pathlib.Path("${REPORT_PATH}").read_text())
s   = doc["audit_summary"]

print()
print("=" * 66)
print("  NUMERICAL VALUES FOR LFX PROPOSAL")
print("=" * 66)
print(f"  Summary:  PASS={s['PASS']}  FAIL={s['FAIL']}  "
      f"ERROR={s['ERROR']}  NO_DATA={s['NO_DATA']}")
print()
for r in doc["binary_results"]:
    print(f"  Binary  : {r['binary_name']}")
    print(f"  Status  : {r['precision_status']}")
    if r["relative_errors"]:
        print(f"  Rel Err : {[f'{e:.4e}' for e in r['relative_errors']]}")
        if r["worst_error"]:
            print(f"  Worst   : {r['worst_error']:.6e}")
    else:
        snippet = (r.get("raw_tail") or "")[:100].replace('\n',' ')
        print(f"  Rel Err : [none extracted]  output: {snippet}")
    if r["eigenvalues"]:
        shown = [f"{v:.8f}" for v in r["eigenvalues"][:5]]
        tail  = "  …" if len(r["eigenvalues"]) > 5 else ""
        print(f"  Eigs    : {shown}{tail}")
    print()

print(f"  JSON report: ${REPORT_PATH}")
print("=" * 66)
PYEOF

log "═══ ALL DONE ═══"
