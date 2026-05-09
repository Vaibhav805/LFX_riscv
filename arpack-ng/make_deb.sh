#!/bin/bash
# Usage: bash make_deb.sh <build_dir>

BUILD_DIR=$1
PKG_NAME="arpack-ng"
VERSION="3.9.1"
ARCH="riscv64"
STAGING_DIR="${PKG_NAME}_${VERSION}_${ARCH}"

# 1. Create directory structure
mkdir -p "$STAGING_DIR/usr/local/lib"
mkdir -p "$STAGING_DIR/DEBIAN"

# 2. Copy the compiled libraries (Looking for both .so and .a)
echo "Searching for libraries in $BUILD_DIR..."
# This finds any .so or .a files inside the build dir
FILES=$(find "$BUILD_DIR" \( -name "*.so*" -o -name "*.a" \) -type f)

if [ -z "$FILES" ]; then
    echo "ERROR: No library files (.so or .a) found in $BUILD_DIR."
    exit 1
fi

for f in $FILES; do
    echo "Adding $f to package..."
    cp "$f" "$STAGING_DIR/usr/local/lib/"
done

# 3. Create the control file
cat <<EOF > "$STAGING_DIR/DEBIAN/control"
Package: $PKG_NAME
Version: $VERSION
Section: libs
Priority: optional
Architecture: $ARCH
Maintainer: Vaibhav Binwal <vaibhav@example.com>
Description: ARPACK-NG for RISC-V compiled for LFX Selection.
EOF

# 4. Build the package
dpkg-deb --build "$STAGING_DIR"

echo "Package created: ${STAGING_DIR}.deb"