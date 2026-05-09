# Toolchain Pitfalls — riscv64 Cross-Compilation on Ubuntu 24.04

## Pitfall 1: CMAKE_SYSROOT Breaks libc Resolution

**Error:**
```
ld: cannot find /usr/riscv64-linux-gnu/lib/libc.so.6 inside /usr/riscv64-linux-gnu
```
**Root cause:** Setting `CMAKE_SYSROOT` overrides the cross-gcc's built-in sysroot. Ubuntu multiarch puts `libc6:riscv64` in `/usr/lib/riscv64-linux-gnu/` — outside the sysroot prefix.  
**Fix:** Omit `CMAKE_SYSROOT` entirely. The cross-gcc already knows its sysroot.

---

## Pitfall 2: Wrong apt Mirror for riscv64

**Error:**
```
Err: http://in.archive.ubuntu.com noble/main riscv64 Packages  404 Not Found
E: Unable to locate package libblas-dev:riscv64
```
**Root cause:** `riscv64` is a "ports" arch. It is **not** on `archive.ubuntu.com`.  
**Fix:**
```bash
sudo dpkg --add-architecture riscv64
echo "deb [arch=riscv64] http://ports.ubuntu.com/ubuntu-ports noble main restricted universe multiverse" \
  | sudo tee /etc/apt/sources.list.d/ubuntu-ports-riscv64.list
sudo apt-get update && sudo apt-get install libblas-dev:riscv64 liblapack-dev:riscv64
```

---

## Pitfall 3: Root-Owned Build Artifacts

**Error:**
```
PermissionError: [Errno 13] Permission denied: final_submission_report.json
CMake Error: Inappropriate ioctl for device
```
**Root cause:** A previous `sudo` run created `_build_riscv64/` owned by root.  
**Fix:**
```bash
sudo chown -R $USER:$USER _build_riscv64/ final_submission_report.json
rm -f _build_riscv64/CMakeCache.txt
```

---

## Pitfall 4: Bash Heredoc Corrupts Python Source

**Error:**
```
Parse error: Invalid control character at: line 1 column 180
```
**Root cause:** `${OUTPUT}` expansion inside a Python heredoc — newlines and Fortran `D`-exponent notation break the Python parser.  
**Fix:** Write binary output to a temp file, read with `open()`:
```bash
qemu-riscv64-static -L "$QEMU_PREFIX" "$BIN" > "$OUT_FILE" 2>&1
python3 extractor.py "$BIN_NAME" "$OUT_FILE"
```

---

## Pitfall 5: ARPACK-ng Has Three Output Formats

| Driver | Format | Where Residual Is |
|--------|--------|------------------|
| `dsbdr*` | `Ritz Value N = X   Relative error = Y` | Explicit label |
| `dndrv*` | `Col N:` then `real  imag  residual` | 3rd number, next line |
| `dsdrv*` | `Col N:` then `eigenvalue  residual` | 2nd number, next line |
