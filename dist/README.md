# Prebuilt binaries

Release artifacts for fstl 0.13.0.

| File | Platform | Notes |
|---|---|---|
| `fstl-0.13.0-linux-x86_64` | Linux x86-64 | Standalone executable. Needs Qt 6 runtime libraries (`libqt6widgets6`, `libqt6opengl6`, ...). |
| `fstl-0.13.0.deb` | Debian/Ubuntu x86-64 | Installer: `sudo apt install ./fstl-0.13.0.deb` (pulls Qt deps, adds the menu entry, icons, and `man fstl`). |
| `fstl-0.13.0-win64.zip` | Windows x86-64 | Unzip and run `fstl\fstl.exe`. Bundles the Qt 6 DLLs; no install needed. MP4 export needs `ffmpeg` on the PATH. |

These were built from this commit. To rebuild:

```bash
# Linux binary + .deb
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
(cd build && cpack)

# Windows zip (cross-compiled via Docker, see cross/)
docker build -t fstl-mingw cross/
docker run --rm -u $(id -u):$(id -g) -e HOME=/tmp -v "$PWD":/src fstl-mingw \
    bash /src/cross/build-windows.sh
```
