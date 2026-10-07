# Prebuilt binaries

Release artifacts for fstl 0.14.0.

| File | Platform | Notes |
|---|---|---|
| `fstl-0.14.0-linux-x86_64` | Linux x86-64 | Standalone executable built against Qt 6.4 / glibc 2.39 (Ubuntu 24.04). Needs Qt 6 runtime libraries (`libqt6widgets6`, `libqt6opengl6`, ...). |
| `fstl-0.14.0.deb` | Debian/Ubuntu x86-64 | Installer: `sudo apt install ./fstl-0.14.0.deb` (pulls Qt deps, adds the menu entry, icons, and `man fstl`). |
| `fstl-0.14.0-win64.zip` | Windows x86-64 | Unzip and run `fstl\fstl.exe`. Bundles the Qt 6.10.3 DLLs and MinGW runtime; no install needed. MP4 export needs `ffmpeg` on the PATH. |

These were built from this commit. To rebuild:

```bash
docker build -t fstl-mingw cross/
# Linux binary + .deb (Ubuntu 24.04 / Qt 6.4 for broad compatibility)
docker run --rm -u $(id -u):$(id -g) -e HOME=/tmp -v "$PWD":/src fstl-mingw \
    bash /src/cross/build-linux.sh      # -> dist-linux/
# Windows zip (MinGW cross-compile, Qt 6.10.3, Wine smoke-tested)
docker run --rm -u $(id -u):$(id -g) -e HOME=/tmp -v "$PWD":/src fstl-mingw \
    bash /src/cross/build-windows.sh    # -> dist-windows/
```
