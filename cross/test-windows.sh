#!/bin/bash
# Smoke-tests the bundled Windows build by running fstl.exe under Wine.
# Catches missing-DLL regressions (e.g. libstdc++-6.dll) and verifies the
# CLI export actually renders. Run inside the cross-build container after
# build-windows.sh. Exits non-zero on failure.
set -uo pipefail

DIST=/src/dist-windows/fstl
EXE="$DIST/fstl.exe"

if ! command -v wine >/dev/null 2>&1; then
    echo "SKIP: wine not installed in this image"
    exit 0
fi
[ -f "$EXE" ] || { echo "FAIL: $EXE not found (run build-windows.sh first)"; exit 1; }

# Wine refuses a prefix whose parent it does not own, so keep HOME and the
# prefix inside the (bind-mounted, user-owned, gitignored) build dir.
export HOME=/src/build-windows/winehome
mkdir -p "$HOME"
export WINEPREFIX="$HOME/.wine"
export WINEDEBUG="${WINEDEBUG:-fixme-all,err-all}"
export WINEDLLOVERRIDES="mscoree,mshtml="  # skip the mono/gecko install prompts
rm -rf "$WINEPREFIX"

# A single Xvfb display for the whole run (wine GUI/Win32 needs a display)
export DISPLAY=:99
Xvfb :99 -screen 0 1024x768x24 >/dev/null 2>&1 &
XVFB_PID=$!
trap 'kill $XVFB_PID 2>/dev/null; wineserver -k 2>/dev/null' EXIT
sleep 2

echo "== initializing wine prefix =="
timeout 120 wineboot -i >/dev/null 2>&1 || true
timeout 30 wineserver -w 2>/dev/null || true

fail=0

# A CLI export is the comprehensive smoke test: it loads the EXE plus
# every bundled DLL (Qt6 Core/Gui/Widgets/OpenGL + the MinGW runtime and
# the platform plugin), renders with OpenGL, and writes a file. A missing
# or broken DLL fails this. (--version is unusable here: a GUI-subsystem
# Windows app shows it in a message box rather than on stdout.)
echo "== fstl.exe --export-png (loads all DLLs, renders, writes) =="
cp /src/gl/sphere.stl "$DIST/sphere.stl"
err=$( ( cd "$DIST" && timeout 120 wine ./fstl.exe sphere.stl --export-png --output-dir . --output smoke ) 2>&1 )
if [ -s "$DIST/smoke.png" ]; then
    echo "   OK: wrote smoke.png ($(stat -c%s "$DIST/smoke.png") bytes)"
    echo "   PASS: Windows bundle launches with all DLLs resolved and renders"
else
    echo "   FAIL: no PNG produced"
    echo "$err" | grep -iE "not found|import|\.dll|could not|platform plugin" | sort -u | sed 's/^/   /'
    fail=1
fi
rm -f "$DIST/sphere.stl" "$DIST/smoke.png"

exit "$fail"
