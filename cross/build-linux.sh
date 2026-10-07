#!/bin/bash
# Runs inside the cross-build container (Ubuntu 24.04, Qt 6.4): builds the
# portable Linux release binary and the .deb into /src/dist-linux.
# Building against the oldest supported Qt 6 / glibc keeps the standalone
# binary and package usable on Ubuntu 24.04+, Debian 12+ and newer distros.
set -euo pipefail

BUILD=/src/build-linux
DIST=/src/dist-linux

cmake -S /src -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DFSTL_BUILD_TESTS=ON \
    -DCMAKE_PREFIX_PATH=/usr/lib/x86_64-linux-gnu/cmake
cmake --build "$BUILD" -j"$(nproc)"

# Headless test run (the GL tests need a platform plugin; Xvfb + Mesa)
if command -v xvfb-run >/dev/null 2>&1 && [ "${SKIP_LINUX_TESTS:-0}" != "1" ]; then
    echo "=== running tests under Xvfb ==="
    (cd "$BUILD" && xvfb-run -a -s "-screen 0 1280x1024x24" ctest --output-on-failure)
fi

(cd "$BUILD" && cpack -G DEB)

VERSION=$(grep -oP 'FSTL_VERSION_MAJOR "\K[0-9]+' /src/CMakeLists.txt).$(grep -oP 'FSTL_VERSION_MINOR "\K[0-9]+' /src/CMakeLists.txt).$(grep -oP 'FSTL_VERSION_PATCH "\K[0-9]+' /src/CMakeLists.txt)
rm -rf "$DIST"; mkdir -p "$DIST"
cp "$BUILD/fstl" "$DIST/fstl-$VERSION-linux-x86_64"
strip "$DIST/fstl-$VERSION-linux-x86_64"
cp "$BUILD/fstl-$VERSION.deb" "$DIST/"
echo "Linux artifacts in dist-linux/:"; ls -la "$DIST"
