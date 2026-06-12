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
# Software-GL fallback for machines with broken OpenGL drivers
cp "$QT_WIN/bin/opengl32sw.dll" "$DIST/" 2>/dev/null || true
cp "$QT_WIN/plugins/platforms/qwindows.dll" "$DIST/platforms/"
cp "$QT_WIN/plugins/styles/"*.dll "$DIST/styles/" 2>/dev/null || true
cp "$QT_WIN/plugins/imageformats/qjpeg.dll" "$DIST/imageformats/" 2>/dev/null || true

VERSION=$(grep -oP 'FSTL_VERSION_MAJOR "\K[0-9]+' /src/CMakeLists.txt).$(grep -oP 'FSTL_VERSION_MINOR "\K[0-9]+' /src/CMakeLists.txt).$(grep -oP 'FSTL_VERSION_PATCH "\K[0-9]+' /src/CMakeLists.txt)
(cd /src/dist-windows && zip -qr "fstl-$VERSION-win64.zip" fstl)
echo "Bundle: dist-windows/fstl-$VERSION-win64.zip"
