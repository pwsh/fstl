#!/bin/bash
# Runs inside the cross-build container: configures, builds, and bundles
# a standalone Windows release into /src/dist-windows.
set -euo pipefail

QT_WIN=/opt/qt/6.4.2/mingw_64
BUILD=/src/build-windows
DIST=/src/dist-windows/fstl

cmake -S /src -B "$BUILD" \
    -DCMAKE_TOOLCHAIN_FILE=/src/cross/toolchain-mingw64.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$QT_WIN" \
    -DQT_HOST_PATH=/usr
cmake --build "$BUILD" -j"$(nproc)"

rm -rf "$DIST"
mkdir -p "$DIST/platforms" "$DIST/styles" "$DIST/imageformats"

cp "$BUILD/fstl.exe" "$DIST/"
for dll in Qt6Core Qt6Gui Qt6Widgets Qt6OpenGL Qt6OpenGLWidgets; do
    cp "$QT_WIN/bin/$dll.dll" "$DIST/"
done
# MinGW runtime DLLs: the EXE links them statically, but the Qt DLLs were
# built against them dynamically, so they must be shipped (version-matched
# copies live alongside the Qt DLLs).
for dll in libstdc++-6 libgcc_s_seh-1 libwinpthread-1; do
    cp "$QT_WIN/bin/$dll.dll" "$DIST/"
done
# Software-GL fallback for machines with broken OpenGL drivers
cp "$QT_WIN/bin/opengl32sw.dll" "$DIST/" 2>/dev/null || true
cp "$QT_WIN/plugins/platforms/qwindows.dll" "$DIST/platforms/"
# Offscreen platform so headless CLI export works on Windows servers too
cp "$QT_WIN/plugins/platforms/qoffscreen.dll" "$DIST/platforms/" 2>/dev/null || true
cp "$QT_WIN/plugins/styles/"*.dll "$DIST/styles/" 2>/dev/null || true
cp "$QT_WIN/plugins/imageformats/qjpeg.dll" "$DIST/imageformats/" 2>/dev/null || true

# Smoke-test the bundle under Wine (catches missing-DLL regressions)
# unless skipped. Fails the build if the bundle does not run.
if [ "${SKIP_WINE_TEST:-0}" != "1" ] && command -v wine >/dev/null 2>&1; then
    echo "=== smoke-testing the bundle under Wine ==="
    bash /src/cross/test-windows.sh || { echo "Windows smoke test FAILED"; exit 1; }
fi

VERSION=$(grep -oP 'FSTL_VERSION_MAJOR "\K[0-9]+' /src/CMakeLists.txt).$(grep -oP 'FSTL_VERSION_MINOR "\K[0-9]+' /src/CMakeLists.txt).$(grep -oP 'FSTL_VERSION_PATCH "\K[0-9]+' /src/CMakeLists.txt)
(cd /src/dist-windows && zip -qr "fstl-$VERSION-win64.zip" fstl)
echo "Bundle: dist-windows/fstl-$VERSION-win64.zip"
