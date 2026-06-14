# Changelog

## 0.13.0 (2026-06-12)

Major modernization and feature release: Qt 6 readiness, bug fixes, image
and animation exporters (GUI and command line), Linux packaging, and
documentation. Version bumped from 0.11.1 (`CMakeLists.txt`).

---

### Build system and library modernization

**Qt 6 support with Qt 5.15 fallback** — `CMakeLists.txt`
- Qt 6 is probed explicitly first (`find_package(Qt6 QUIET ...)`) with a
  Qt 5.15 fallback. Qt 6 builds additionally link `Qt6::OpenGLWidgets`
  (where `QOpenGLWidget` moved in Qt 6).
- The conventional `find_package(QT NAMES Qt6 Qt5 ...)` idiom was tried
  first and **silently selected Qt 5 even with Qt 6 installed**: CMake
  scans config directories in filesystem order and `cmake/Qt5/` sorts
  before `cmake/Qt6/`, beating the NAMES preference. Diagnosed with
  `CMAKE_FIND_DEBUG_MODE` and worked around with the explicit probe (a
  comment in CMakeLists.txt documents the pitfall).
- Verified against Qt 6.4.2 (Ubuntu `qt6-base-dev`): compiles with zero
  warnings, and the GUI, export test harness, and CLI PNG/GIF exports all
  pass; output is visually identical to the Qt 5.15 build. Qt 5.15 builds
  remain warning-free.
- *Why:* Qt 5 is in legacy support; the codebase used several APIs removed
  or deprecated in Qt 6. The fallback keeps the project building on
  distros that still ship only Qt 5.
- C++ standard raised from 14 to 17; minimum CMake 3.10 → 3.16. Also fixed
  a latent typo: the old file set `CXX_STANDARD_REQUIRED` (missing the
  `CMAKE_` prefix), which silently did nothing.
- Resource compilation switched from `qt5_add_resources()` + manual
  `SKIP_AUTOGEN` workarounds to `CMAKE_AUTORCC`, which works identically
  under both Qt versions.
- Linking now uses imported targets `OpenGL::GL` and `Threads::Threads`
  instead of raw variables.
- Windows/MSVC install rules were parameterized over the Qt major version;
  Qt 6 uses `qt_generate_deploy_app_script()` (windeployqt) instead of
  hand-copying DLLs.

**Qt 6 API compatibility across the sources**
- `src/window.cpp` — key combinations `Qt::CTRL + Qt::SHIFT + Qt::Key_C`
  and `Qt::ALT + Qt::Key_S` changed to use `|` (the `+` overloads were
  removed in Qt 6).
- `src/canvas.cpp` — mouse positions read through a small
  `mouse_position()` helper: `QMouseEvent::position()` on Qt 6,
  `pos()` on Qt 5 (each version deprecates the other's accessor).
  `QWheelEvent::position()` is used directly (present since 5.14).
- `src/shaderlightprefs.cpp` — all string-based `SIGNAL()/SLOT()` connects
  converted to compile-time-checked pointer-to-member connects
  (`qOverload<int>` for `QComboBox::currentIndexChanged`).
- `src/canvas.h`, `src/mesh.h`, `src/vertex.h`, `src/loader.cpp`,
  `src/window.cpp`, `src/shaderlightprefs.cpp` — the heavyweight
  `<QtOpenGL>` / `<QtOpenGL/QtOpenGL>` umbrella includes (whose contents
  changed incompatibly in Qt 6) were replaced with the precise headers each
  file actually uses; `vertex.h`/`mesh.h` need only `<qopengl.h>` for the
  GL typedefs. Files that had been relying on transitive includes
  (`loader.cpp`, `window.cpp`, `shaderlightprefs.cpp`) received explicit
  ones, and `loader.h` gained a `class QFile;` forward declaration.
- Deprecations flagged by the Qt 6.4 compiler were fixed so both builds
  are warning-free: `matrix * vector` replaced with `matrix.map(vector)`
  (`src/canvas.cpp` mouse-pan and zoom-about-cursor math, `src/axis.cpp`
  label placement; `map()` has identical semantics in Qt 5), and
  `parallel_sort` in `src/loader.cpp` now takes `verts.data()` pointers
  instead of relying on the deprecated implicit `QList::iterator` →
  raw-pointer conversion.

---

### Bug fixes

**Startup race: mesh loaded before GL initialization (crash)** —
`src/canvas.cpp`, `src/canvas.h`
- If the loader thread finished before the first paint created the OpenGL
  context, `Canvas::load_mesh()` constructed a `GLMesh` with no current
  context and dereferenced the *uninitialized* `axis` pointer. The mesh is
  now stashed in `pending_mesh` and uploaded at the end of
  `initializeGL()`; `load_mesh()` also calls `makeCurrent()` so buffer
  uploads are always valid. Discovered when the new test harness segfaulted
  on exactly this path; the GUI had been winning the race by luck.

**Uninitialized Canvas members (potential crash / undefined behavior)** —
`src/canvas.h`, `src/canvas.cpp`
- `drawMode`, `perspective`, `drawAxes`, `invertZoom`,
  `resetTransformOnLoad`, `backdrop`, `axis`, `mesh_vertshader` were never
  initialized; with garbage `drawMode`, `draw_mesh()` selected no shader
  and called `bind()` on NULL. All members now have in-class or
  constructor-list initializers, and `draw_mesh()` defaults to the shaded
  shader instead of NULL.

**Screenshot dialog cancel wrote a stray file** — `src/window.cpp`
- Cancelling *Save Screenshot* produced an empty filename, which the
  extension fix-up turned into a file literally named `.png` in the
  working directory. Now returns early on cancel. The hand-rolled
  reverse-iterator extension parser was replaced with
  `QFileInfo::suffix().toLower()` (also fixing case-sensitive comparison,
  so `IMAGE.PNG` no longer gets `.png` appended).

**Settings written on every window move/resize** — `src/window.cpp`,
`src/window.h`, `src/shaderlightprefs.cpp`, `src/shaderlightprefs.h`
- The main window and the shader-prefs dialog wrote their geometry to
  `QSettings` inside `resizeEvent`/`moveEvent` — dozens of registry/disk
  writes per second while dragging a window. Following the standard Qt
  pattern, geometry is now saved once: in `Window::closeEvent()` for the
  main window and `ShaderLightPrefs::hideEvent()` for the dialog (which is
  hidden, not destroyed, on close). Restore logic is unchanged. Trade-off:
  geometry from a session that crashes (rather than closes) is not saved.

**ASCII STL parser: out-of-bounds read and swallowed errors** —
`src/loader.cpp`
- `vertex` lines were split and indexed `line[1..3]` without checking the
  token count — a truncated line crashed the loader thread. Now bounds
  checked.
- The three `toFloat(&okay)` calls each overwrote `okay`, so a bad X or Y
  coordinate was accepted if Z parsed. Each component now has its own
  status flag and all three must succeed.

---

### Performance

**~100 ms removed from every file open** — `src/loader.cpp`
- `load_stl()` polled the file size in 100 ms sleeps until it stopped
  changing, on *every* load. That guard only matters when autoreload fires
  while a slicer/CAD tool is mid-write, so it now runs only when
  `is_reload` is true.

**Scroll-zoom loop replaced with `std::pow`** — `src/canvas.cpp`
- `wheelEvent` multiplied `zoom` by 1.001 once per wheel-delta unit
  (typically 120 iterations per notch, thousands for fast scrolls or
  high-resolution wheels). Replaced by a single mathematically identical
  `std::pow(1.001f, ±delta)`.

**Loader hot loops** — `src/loader.cpp`, `src/mesh.cpp`
- Vertex deduplication and flattening loops iterate by `const auto&`
  instead of copying each 16-byte `Vertex` (relevant on multi-million
  triangle meshes).
- `Mesh::min/max` use float `std::min/std::max` instead of double-precision
  `fmin/fmax` and skip the redundant first-element comparison.
- `read_stl_ascii` no longer constructs its vertex vector with a
  pointless `tri_count * 3` size expression that was always zero.

---

### GUI feature: rotation exports (PNG sequence and animated GIF)

**Frame capture API** — `src/canvas.cpp`, `src/canvas.h`
- `Canvas::grabRotatedFrame(angle, axis)` pre-multiplies a view-space
  rotation onto the current orientation, renders offscreen via
  `grabFramebuffer()`, and restores the on-screen transform. Pre-
  multiplication means the model spins about the requested *screen* axis
  no matter how the user has oriented it.

**Export dialogs** — `src/exportdialog.h`, `src/exportdialog.cpp` (new)
- `PngExportDialog` (axis, image count, total sweep) and `GifExportDialog`
  (axis, loop/bounce mode, sweep, endpoint angles, degrees/frame, FPS,
  output width).
- `RotationExportOptions` holds the parameters and generates the angle
  sequences (`pngAngles()`, `gifAngles()`); it is deliberately
  widget-free so the CLI reuses the identical sequencing logic. Bounce
  sequences exclude the duplicated endpoints on the return leg so the
  loop is seamless; full-circle PNG sweeps divide by N (the endpoint
  duplicates frame 0) while partial sweeps divide by N−1 to include both
  ends.

**Menu actions and export drivers** — `src/window.cpp`, `src/window.h`
- *File → Export Rotation PNGs…* and *File → Export Rotating GIF…* with
  progress dialogs and cancel support (a cancelled GIF deletes the partial
  file). GIF frames are scaled to the chosen width and encoded with the
  vendored `gif.h`.

---

### Command-line export mode

**Offscreen renderer** — `src/offscreenrenderer.h`,
`src/offscreenrenderer.cpp` (new)
- Renders without any window using `QOffscreenSurface` +
  `QOpenGLFramebufferObject`, reusing the existing `:/gl/mesh.vert` and
  `:/gl/mesh_light.frag` shaders; the model color is fed through the
  lighting uniforms (60% ambient + 40% directional of the same tint).
- `layoutForAngles()` projects the mesh bounding-box corners through the
  full transform for *every* requested angle, then sizes the output to the
  swept screen rectangle: the model is centered on x/y, never clips while
  spinning, and the aspect ratio follows the model rather than the window.
- Background is a clear-color, so any color including fully or partially
  transparent works; the FBO is premultiplied, handled when converting out.
- *Why a separate renderer:* `QOpenGLWidget::grabFramebuffer` requires a
  realized widget (and thus a window); a CLI tool must not flash windows
  and should run under `xvfb` on servers.

**CLI driver** — `src/cliexport.h`, `src/cliexport.cpp` (new);
wired in `src/main.cpp`
- `cli_export_requested()` scans raw argv for `--export-png`/`--export-gif`
  before any `QApplication` exists; CLI mode uses a `QGuiApplication` and
  never creates the main window. With no `DISPLAY`/`WAYLAND_DISPLAY`, it
  falls back to Qt's `offscreen` platform.
- Switches (see README for the full reference): `--export-png`,
  `--export-gif`, positional inputs / `-i/--input` / `--input-dir`
  (batch), `-o/--output`, `--output-dir`, `--color` (default white),
  `--bg` (default transparent), `--width`/`--height` (defaults: PNG max
  1024 px, GIF max 640 px, aspect preserved), `--axis` (default y),
  `--angles` (PNG, default straight-on 0), `--sweep`/`--step`/`--fps`
  (GIF loop, defaults 360/3/25), `--bounce` with `--from`/`--to`
  (defaults −90/+90).
- stdin pipeline support: input `-` spools stdin to a `QTemporaryFile`
  for the existing loader, so `cat model.stl | fstl - --export-png ...`
  works for binary and ASCII STL.
- Mesh loading reuses the GUI's `Loader` synchronously (direct-connected
  signals, `run()` called inline) so both paths share one parser.
- Exit codes: 0 success, 1 any input failed, 2 argument errors. Batch
  failures are reported per file and don't abort the run.

**GUI argument handling** — `src/app.cpp`
- `App` now opens the first *non-flag* argument instead of blindly
  `args.at(1)`, so GUI switches aren't misread as filenames.

---

### Vendored: gif.h GIF encoder (with transparency patch)

- `src/gif.h` (new) — Charlie Tangora's public-domain single-header GIF
  encoder, chosen because Qt cannot write GIFs and a dependency-free
  header keeps the build simple.
- Local patches (marked `fstl patch` in the file):
  - All free functions marked `inline` — the header is included from two
    translation units (`window.cpp`, `cliexport.cpp`) and would otherwise
    produce duplicate symbols.
  - Real transparent-background support: upstream ignores alpha and uses
    the transparent palette index only for inter-frame delta encoding.
    A new `transparent` flag on `GifWriter`/`GifBegin()` disables delta
    encoding, maps source pixels with alpha < 128 to the transparent
    index, and sets the frame disposal method to "restore to background"
    (0x09) so rotated frames don't ghost over each other.

---

### Launch behavior fixes

**Terminal is released on GUI launch** — `src/main.cpp`
- The GUI now forks and detaches (`fork()` + `setsid()`) before the
  `QApplication` is created, so the shell prompt returns immediately and
  the viewer survives the terminal closing. `-f`/`--foreground` opts out
  (debugging, or scripts that must wait). CLI export mode never detaches.

**`XDG_RUNTIME_DIR` warning silenced** — `src/main.cpp`
- When the variable is unset (sudo shells, bare environments), fstl
  creates a private `/tmp/fstl-runtime-<uid>` (mode 0700) and sets the
  variable itself instead of letting Qt warn about its shared fallback.

---

### Packaging and deployment (Linux)

- `CMakeLists.txt` — Linux installs now include the desktop entry
  (`xdg/fstlapp-fstl.desktop`, providing the menu entry and `model/stl`
  file association) and the hicolor icons at 7 sizes, in addition to the
  binary.
- CPack: packages install to `/usr` (Debian convention) instead of
  `/usr/local`; DEB metadata added (maintainer, section `graphics`,
  homepage); `CPACK_DEBIAN_PACKAGE_SHLIBDEPS` derives runtime dependencies
  from the actually-linked libraries; the RPM generator is only enabled
  when `rpmbuild` exists so `cpack` doesn't fail on Debian-only hosts.

---

### Second review pass (code quality, memory, GUI polish)

**Binary STL loader: streamed reads and overflow fix** — `src/loader.cpp`
- The whole file body (50 bytes/triangle) was buffered before parsing,
  briefly adding ~250 MB of peak memory for a 5M-triangle model on top of
  the vertex array. The body is now streamed in 800 KB chunks (16384
  triangles), with short reads reported as bad STL.
- The file-size validation `84 + tri_count * 50` was computed in 32-bit
  arithmetic and overflowed for models above ~85M triangles, rejecting (or
  in pathological cases accepting) valid/invalid files; now `qint64`.
- The ASCII reader reserves vertex capacity from the file size (~250
  bytes/facet heuristic, capped) to avoid repeated reallocation.

**Canvas resource ownership** — `src/canvas.h`, `src/canvas.cpp`
- `mesh`, `backdrop`, `axis`, `mesh_vertshader`, and `pending_mesh` are
  now `std::unique_ptr` instead of raw pointers with manual `delete`,
  removing leak risk on early-exit paths; destruction still happens with
  the GL context current. Also added the missing `break` after the
  `backview` case (harmless fallthrough, now explicit).

**GUI exports no longer capture HUD overlays** — `src/canvas.cpp`,
`src/canvas.h`
- `grabRotatedFrame()` previously captured the canvas verbatim, so
  exported PNG/GIF frames could include the axes overlay and the
  mesh-info/status text if enabled. A `hideHud` guard now suppresses the
  axes and `QPainter` text during export grabs (the backdrop is kept, as
  it is part of the scene's look).

**GUI polish** — `src/window.cpp`, `src/exportdialog.cpp`,
`src/exportdialog.h`
- Keyboard shortcuts: Ctrl+S (screenshot), Ctrl+E (rotation PNGs),
  Ctrl+G (rotating GIF); registered on the window so they work with the
  menu bar hidden.
- Drag-and-drop accepted only lowercase `.stl` names; `.STL` files (as
  produced by several CAD tools) were silently rejected. Now
  case-insensitive.
- Both export dialogs persist their last-used values in `QSettings`
  (`exportPng/*`, `exportGif/*`) and restore them on open.
- `on_drawMode()` initialized its mode variable (was uninitialized if the
  sender matched no known action).

**Small fixes** — `src/shaderlightprefs.cpp`, `src/cliexport.cpp`,
`src/window.cpp`
- `QLineEdit::setValidator()` does not take ownership; the two
  `QDoubleValidator`s were created parentless and leaked. Now parented to
  their line edits.
- CLI gained `--version` (`-v`).
- Recent-files rebuild iterates by const reference.

### Help and packaging metadata

**No-GUI `--help`/`--version`** — `src/cliexport.cpp`
- `fstl --help`, `--help-all`, and `-v/--version` now run in command-line
  mode and exit without ever opening (or forking) the GUI; the help text
  gained a GUI-mode synopsis and a pointer to fstl(1).

**Help menu** — `src/window.cpp`, `src/window.h`
- New *Help > Usage and Controls* (F1): a modeless dialog documenting the
  mouse controls, every keyboard shortcut, each menu item, and the
  command-line switches. The About dialog was expanded to describe the
  exporters and point at the usage dialog and man page.

**Man page** — `man/fstl.1` (new), `CMakeLists.txt`
- Full fstl(1) covering both modes, all switches with defaults, examples,
  exit codes, and the headless-GL note; gzipped at build time (with a
  plain-install fallback) and installed to `share/man/man1`.

**Software-center metadata** — `xdg/fstlapp-fstl.metainfo.xml` (new),
`CMakeLists.txt`
- AppStream metainfo (validates clean with `appstreamcli`): rich
  description, feature lists, categories, URLs, release notes — this is
  what GNOME Software / App Center displays for the installed package.
- The Debian package's one-line description was replaced with a proper
  synopsis plus a three-paragraph long description (viewer features,
  export features); fixed an initial attempt that duplicated the synopsis
  because CPackDeb supplies it from `CPACK_PACKAGE_DESCRIPTION_SUMMARY`
  and indents the body itself.

### Continuous rotation, video recording, and rebindable keys

**Momentum spin** — `src/canvas.cpp`, `src/canvas.h`, `src/window.cpp`
- New *View > Momentum Spin* (off by default, persisted): releasing a
  left-drag keeps the model rotating with the drag's angular velocity and
  trajectory (view-space axis from the arcball math, speed capped at
  720°/s, only when the release comes straight out of an active drag).
  Clicking stops the spin. Driven by a 16 ms QTimer in Canvas.

**Animate Rotation dialog** — `src/animatedialog.h`, `src/animatedialog.cpp`
(new); *View > Animate Rotation...*
- Per-axis X/Y/Z speeds in °/s (default: slow 20°/s Y turntable),
  modeless with Play / Record / Stop. Setting changes apply live during
  playback; an optional randomizer re-rolls speeds every 2.5 s (zeroing
  roughly one axis in three so the axis visibly changes) and shows the new
  values in the spinboxes in real time.
- Angles accumulate from the **default orientation**, never the current
  one — Play resets the view first, so identical settings always
  reproduce identical motion (covered by a new harness check).
- Record streams the live animation to MP4 through ffmpeg (rawvideo RGBA
  over stdin → libx264 yuv420p at 30 fps); a friendly error explains how
  to install ffmpeg if it's missing. Closing/hiding the dialog stops
  playback and finalizes any recording.

**Rotation MP4 export** — `src/exportdialog.{h,cpp}` (`Mp4ExportDialog`),
`src/window.cpp` (*File > Export Rotation MP4...*, Ctrl+M)
- Offline (frame-exact, not realtime) export: duration, fps, max width,
  and **total degrees per axis**, each allowed beyond 360° — per-axis
  speed follows from degrees/duration, so X=720° with Y=360° rotates X
  twice as fast as Y. Frames render via the same default-orientation
  animation API and stream to ffmpeg; encoder back-pressure is bounded at
  64 MB so a slow encode can't balloon memory; cancel kills the encode
  and removes the partial file. Settings persist.

**Configurable keyboard shortcuts** — `src/keybindingsdialog.{h,cpp}`
(new), `src/window.cpp` (*View > Configure Keyboard Shortcuts...*)
- New keyboard movement actions (auto-repeat, so holding a key moves
  continuously): rotate W/A/S/D, roll Q/E, pan Shift+W/A/S/D, zoom +/−,
  backed by new Canvas methods `rotateView`/`rollView`/`panView`/`zoomView`.
- File navigation (Left/Right, previously hard-coded in `keyPressEvent`)
  became rebindable actions, alongside open, reload, screenshot, the
  three exporters, fullscreen, and hide-menu-bar.
- The dialog edits each binding with `QKeySequenceEdit`, warns on
  duplicates, offers Restore Defaults, and persists only non-default
  bindings (`keys/*` in QSettings).

**Animation refinements** — `src/animatedialog.{h,cpp}`,
`src/canvas.{h,cpp}`
- Random mode changes now fire at a **random interval** between
  user-set minimum and maximum seconds (defaults 2-5 s) instead of a
  fixed cadence, and all changes **ease smoothly** (exponential
  approach, ~1.5/s) rather than snapping.
- Random mode no longer resets the model: it animates from the current
  view (the animation API gained base-orientation overloads). Fixed-speed
  playback still restarts from the default orientation for
  reproducibility; toggling random mid-play rebases without a jump.
- **Recording defaults next to the source file**, named
  `<model>_x<sx>_y<sy>_z<sz>.mp4` from the axis speeds, or
  `<model>_random.mp4` in random mode (existing files get `_2`, `_3`,
  ... rather than being overwritten). The save dialog only appears if
  that location isn't writable.
- **Random color cycling** for the model, light, and background colors
  (independent checkboxes), blended smoothly through a user-defined
  palette of colors - or fully random colors when the palette is empty.
  Color/light animation temporarily switches to the lit draw mode and
  restores the previous mode on stop; overrides are transient and never
  touch the saved settings.
- **Light source controls** in the dialog: a direction selector (the
  same 26 named directions as Draw Mode Settings) and a "randomly move
  the light" option that glides the light smoothly between random
  directions at a **user-set movement speed** (constant-speed travel,
  default 0.1 units/s - initially 0.4, which proved much too fast;
  saved settings still carrying 0.4 are migrated), with **per-axis min/max range limits** constraining where
  the random targets land and a **live position readout** of the light's
  current x/y/z while it moves.

**Animation dialog rework: per-channel palettes, palette locking,
timed transitions, collapsible UI** — `src/animatedialog.{h,cpp}`
- *Palette lock fix:* color animation previously seeded from the current
  canvas color, so recordings opened on (e.g.) the default blue
  background fading toward the palette. With a palette set, the starting
  color now snaps to a palette color and every transition endpoint comes
  from the palette - nothing outside it is ever shown as an endpoint.
- *Per-channel palettes:* the model, light, and background colors each
  have their own independent palette, enable checkbox, and transition
  settings (the old single shared palette is gone).
- *Smoother, longer transitions:* color changes are time-based
  smoothstep blends spanning the entire interval, with a per-channel
  "Transition time" min/max range (defaults 3-8 s); the next transition
  begins the moment one completes, so motion is continuous. Random
  rotation speeds glide the same way across their change interval
  instead of the previous exponential snap.
- *Per-axis driving and free movement:* a master "Animate rotation"
  switch plus a per-axis "Randomly change this axis" option replace the
  single random checkbox. During playback, mouse drags are no longer
  overwritten: each tick, the user's rotation delta is folded into the
  animation base with its component along every *driven* axis removed
  (an axis is driven when rotation is enabled and it is random or has a
  non-zero speed). The result: rotation off = fully free view while
  colors/light animate; axes at 0 = free; driven axes = locked to the
  animation. Recordings of free playback are suffixed `_free`.
- *Per-axis rotation ranges:* each axis (X/Y/Z) has its own collapsible
  sub-section with an independent speed range (defaults -60 to 60 °/s)
  and change-interval range (defaults 2-5 s); each axis transitions on
  its own schedule, so e.g. Y can re-roll every few seconds while X
  drifts slowly.
- *Collapse packing:* the dialog uses a fixed-size layout constraint so
  collapsing a section shrinks the dialog instead of spreading the
  remaining sections over the old height.
- *Light movement* now picks its next random target the moment it
  arrives (continuous travel) rather than on a shared timer.
- *Collapsible sections:* the dialog is organized into expandable
  Rotation / Model color / Light color / Background color / Light source
  sections (Rotation expanded by default) to keep the growing option set
  manageable.
- *Defaults button* restores every animation setting to its built-in
  default. (A settings-migration shim for an earlier light-speed default
  was removed in favor of this; the codebase carries no user-specific
  migration logic.)

**Light brightness** — `src/canvas.{h,cpp}`,
`src/shaderlightprefs.{h,cpp}`, `src/animatedialog.{h,cpp}`
- A persisted overall brightness (0-3, default 1.0) multiplies both the
  ambient and directive factors of the lit draw mode. Exposed everywhere
  light is configurable: a Brightness row with Reset in *Draw Mode
  Settings*, and a live Brightness control in the Animate Rotation
  light group - both edit the same shared value.

**Background color picker** — `src/shaderlightprefs.{h,cpp}`,
`src/canvas.{h,cpp}`, `src/window.cpp`
- *View > Draw Mode Settings* gained a Background row: pick a flat
  viewport background color (persisted) or Reset to the original
  gradient. Applies to every draw mode, so the settings dialog is now
  always enabled rather than only in the lit mode.

**Packaging** — `CMakeLists.txt`: the Debian package now `Suggests:
ffmpeg`. Help dialog, About, man page, and README document all of the
above.

### Windows: bundle MinGW runtime DLLs + automated smoke test

- **Fix "libstdc++-6.dll was not found" on launch** - `cross/build-windows.sh`:
  the EXE links the C++ runtime statically, but the Qt 6 DLLs were built
  against the MinGW runtime dynamically, so `libstdc++-6.dll`,
  `libgcc_s_seh-1.dll`, and `libwinpthread-1.dll` (version-matched copies
  from the Qt bin dir) are now shipped in the bundle, along with the
  offscreen platform plugin.
- **CLI offscreen fallback was wrong on Windows** - `src/main.cpp`: the
  "no DISPLAY -> force QT_QPA_PLATFORM=offscreen" logic fired on Windows
  (which has neither DISPLAY nor WAYLAND_DISPLAY), selecting a platform we
  did not ship. It is now guarded to Unix only; Windows always uses the
  windows platform.
- **Automated Windows testing** - `cross/test-windows.sh` (new),
  `cross/Dockerfile`: the build image gained Wine + Xvfb, and the build
  now smoke-tests the bundle by running `fstl.exe` under Wine - a CLI PNG
  export that loads every bundled DLL, renders with OpenGL, and writes a
  file (catches exactly this missing-DLL class of regression; the build
  fails if it fails, skippable with SKIP_WINE_TEST=1). The Wine render is
  byte-identical to the Linux render (20398-byte sphere PNG).

### Windows cross-build

**Standalone Windows release** — `cross/` (new: `Dockerfile`,
`toolchain-mingw64.cmake`, `build-windows.sh`), `CMakeLists.txt`
- `docker build -t fstl-mingw cross/ && docker run --rm --user $(id -u):$(id -g) -e HOME=/tmp -v "$PWD":/src fstl-mingw bash /src/cross/build-windows.sh`
  produces `dist-windows/fstl-<version>-win64.zip`: a standalone
  `fstl.exe` (PE32+, x86-64) bundled with the official Qt 6.4.2 MinGW
  DLLs, the `qwindows` platform plugin, the Vista style, the JPEG image
  plugin, and `opengl32sw.dll` as a software-GL fallback. The MinGW
  runtime is linked statically, so the binary depends only on Windows
  system DLLs plus the bundled Qt (verified with objdump).
- Cross-build specifics solved along the way: native host Qt tools
  (same 6.4.2 version) drive AUTOMOC/AUTORCC/AUTOUIC since the target
  package ships Windows-only tools; `rcc` is forced to zlib compression
  because Ubuntu's rcc defaults to zstd which the official Qt MinGW
  libraries don't export; and the icon `.rc` is now compiled with
  windres on MinGW (`enable_language(RC)`) instead of the MSVC-only
  trick of passing the `.rc` path as a link flag (which broke non-MSVC
  Windows builds - a latent upstream bug).
- Caveats: built and dependency-checked on Linux but not executed (no
  Windows/wine here) - worth a smoke test on a real Windows box; MP4
  export/recording on Windows requires `ffmpeg.exe` on the PATH.

### Transparency, settings files, and CLI pipeline parity

**Model opacity** — `gl/*.frag`, `src/canvas.{h,cpp}`,
`src/shaderlightprefs.cpp`, `src/animatedialog.{h,cpp}`,
`src/offscreenrenderer.{h,cpp}`
- All four fragment shaders gained a `model_alpha` uniform; the canvas
  and offscreen renderer enable blending when opacity < 1
  (`glBlendFuncSeparate` so premultiplied output keeps correct alpha -
  a plain `glBlendFunc` halved the alpha of translucent pixels over
  transparent backgrounds, caught by a pixel check).
- Persisted Opacity row (with Reset) in *Draw Mode Settings*; a "Model
  transparency" section in the animation dialog randomly glides opacity
  within a user min/max range on its own transition-time range, via a
  transient override that never touches the saved setting.

**Light Z range default** — `src/animatedialog.cpp`: the random light
movement's Z minimum now defaults to 0 so the light stays in front of
the object (X/Y still default to -1..1).

**Settings export/import** — `src/window.{h,cpp}`
- *File > Export Settings... / Import Settings...* write/read an
  `.ini` with every fstl setting except window geometry. Import applies
  window-level settings immediately; dialogs pick theirs up when
  reopened.

**CLI: full appearance control, MP4 export, and settings files** —
`src/cliexport.cpp`, `src/offscreenrenderer.{h,cpp}`
- New switches: `--opacity`, `--brightness`, `--light-dir x,y,z`, and
  `--settings <file.ini>` (defaults flow from an exported settings file;
  explicit switches override). The offscreen renderer takes a
  `RenderStyle` struct carrying all of it.
- `--export-mp4` with `--duration`, `--rx/--ry/--rz` (per-axis totals,
  may exceed 360°, speeds follow) and shared `--fps`/`--width` renders
  frame-exact videos through ffmpeg, combinable with `--export-png`/
  `--export-gif` in one run - so a single command can generate images,
  previews, and movies for whole directories:
  `fstl --input-dir models --export-png --export-mp4 --settings style.ini`.
  Verified end-to-end (h264 output validated with ffprobe; settings
  precedence and opacity alpha checked per pixel).

### Statistics dialog: avoid the "fstl is ready" notification

- The earlier terminal-only-fork change did not fix it: the popup is
  gnome-shell's internal window-attention handler, fired on the native
  Wayland menu-click activation path (not reproducible programmatically
  or via D-Bus). It was unique to the Statistics dialog. To eliminate
  every way that dialog differed from the dialogs that do not trigger it
  (`src/statisticsdialog.{h,cpp}`, `src/window.{h,cpp}`): the dialog is
  now created fresh on demand (WA_DeleteOnClose, like the Usage dialog)
  instead of kept as a hidden-then-reshown window; it no longer touches
  the canvas in its constructor (the persisted overlay is restored at
  startup via `StatisticsDialog::applySaved()`); and the `QGroupBox`
  wrapper was removed in favor of a plain checkbox list.
- **Bounded the live stats repaint** - `src/canvas.{h,cpp}`: FPS and
  rotation-speed previously kept the canvas repainting as fast as
  possible (an unbounded `update()` loop). It now refreshes on a ~30 Hz
  timer while those stats are shown - much lighter, and avoids a
  continuous render loop that could interfere with compositor focus.

### Fix: spurious "fstl is ready" desktop notification

- **Only fork-detach when launched from a terminal** - `src/main.cpp`:
  the background fork-on-launch (which frees the terminal so the shell
  prompt returns) ran for every GUI launch too. On a desktop launch
  there is no terminal to release, and forking detaches the process from
  the desktop's launch tracking, so GNOME showed a "'fstl' is ready"
  window-attention notification when a dialog (e.g. Statistics) later
  mapped. The detach now happens only when stdin/stdout/stderr is a tty
  (a real terminal launch); GUI launches stay attached. Verified both
  paths: non-tty launch no longer forks, tty launch still detaches.

### Statistics moved to a dialog

- **View > Statistics... dialog** - `src/statisticsdialog.{h,cpp}` (new),
  `src/window.{h,cpp}`: the statistics submenu of checkboxes is replaced
  by a dialog with a master "Show statistics overlay" switch plus the ten
  item checkboxes. The master toggles the whole overlay on/off (off by
  default); items are greyed out while it is off but keep their
  selection. Applies live and persists (statistics/enabled + items).

### Momentum spin back to flick velocity

- Reverted the fixed/configurable momentum speed: the spin again
  continues at the velocity (and along the trajectory) of the final drag
  motion, so a faster flick spins faster, as it originally did. The
  speed submenu/presets were removed. `src/canvas.{h,cpp}`,
  `src/window.cpp`. (Speed is capped at 720 deg/s and needs a flick of
  >30 deg/s to start.)

### Momentum spin speed is configurable

- The fixed momentum speed (60 deg/s) was much slower than the old
  flick-based spin could reach, so fast spins were no longer possible.
  The speed is still static (not derived from the flick) but is now
  adjustable and persisted via **View > Momentum Spin > Speed** with
  presets Slow (90), Medium (180), Fast (360, the new default), and Very
  fast (720 - the old maximum). `src/canvas.{h,cpp}`, `src/window.cpp`.

### Momentum spin uses a fixed speed

- **Static momentum speed** - `src/canvas.{h,cpp}`: releasing a drag with
  Momentum Spin enabled previously continued at the flick velocity
  (`angle / dt` at the instant of release), so the spin rate varied with
  how fast the mouse happened to be moving. It now spins at a fixed
  internal speed (`momentumSpeed`, 60 deg/s) along the drag's trajectory
  - the direction still follows the drag, but the speed is consistent
  every time. Verified: a slow flick and a 10x-faster flick both spin at
  ~60 deg/s.

### View > Statistics overlay

- **Decoupled the info text from Draw Axes** - `src/canvas.{h,cpp}`:
  Draw Axes previously also drew the triangle count and bounding box as a
  text overlay. It now shows only the 3D axes and the corner orientation
  hud.
- **New View > Statistics submenu** - `src/window.{h,cpp}`,
  `src/canvas.{h,cpp}`: ten independently-toggleable readouts, all off by
  default and persisted - triangle count, bounding box, model size,
  orientation (pitch/yaw/roll), rotation speed (per-axis deg/s derived
  from frame-to-frame orientation change), frame rate (with a live render
  loop while shown), zoom/projection, draw mode, colors (model/light/
  opacity, honoring animation overrides), and lighting (brightness +
  direction). Rendered as a top-left overlay and excluded from exports.

### View > Up Axis toggle

- **Z-up / Y-up viewpoint convention** - `src/canvas.{h,cpp}`,
  `src/window.{h,cpp}`: the viewpoint presets (Top, Front, ...) and the
  default load orientation assume the model's up axis is Z (the STL /
  3D-printing convention). A model authored Y-up therefore showed Top and
  Front swapped. A new **View > Up Axis** submenu (Z up / Y up, persisted)
  applies a Y-up to Z-up correction (a 90 degree X pre-rotation) to the
  presets and default view, and re-orients immediately. Verified with an
  asymmetric test box: under Y-up, Top and Front render the same faces
  that Front and Top do under Z-up.

### Documentation: complete switch reference and interface screenshots

- **README / man page / `--help` audited for completeness**: a
  source-driven cross-check confirms all 31 command-line switches now
  appear in both the README tables and `fstl(1)`. Several added in recent
  releases (`--export-mp4`, `--opacity`, `--brightness`, `--light-dir`,
  `--settings`, `--duration`, `--rx/--ry/--rz`) were missing from the
  README's structured tables and are now documented with arguments,
  defaults, and behavior; `--help-all` was added to the man page.
- **Interface screenshots** — `docs/images/`, README "Interface"
  section: the main window, every menu and View submenu, Draw Mode
  Settings, the Animate Rotation dialog, and the three export dialogs.
- **`fstl_screenshot_gen`** — `test/screenshot_gen.cpp`, a new
  `-DFSTL_BUILD_TESTS=ON` target that regenerates the screenshots from
  the running widgets (compositing the GL framebuffer into the window
  grab, since `QWidget::grab()` does not capture `QOpenGLWidget`
  content). Keeps the images reproducible and current.

### Crash fix: SIGSEGV on close

- **Close-time crash** — `src/animatedialog.{h,cpp}`: closing fstl
  segfaulted in `QWidget::update()`. The animation dialog's destructor
  (added in the prior review for clean recording shutdown) calls
  `stop()`, which dropped the canvas's animation overrides via
  `canvas->...->update()`. But the dialog and the Canvas are both
  children of the main window, and the Canvas is destroyed first during
  teardown, so `stop()` dereferenced a dangling pointer. Fixed by
  holding the canvas as a `QPointer` (auto-nulls on destruction) and
  guarding the canvas access in `stop()`. Added `test/test_teardown.cpp`
  (the `fstl_test_teardown` target) which builds a Window, shows it, and
  destroys it; it reproduced the crash and now passes.

### Third review pass

Compiler-assisted (`-Wall -Wextra`) and manual review after the
animation/transparency/CLI expansion. Fixed:

- **Palette swatch crash risk** — `src/animatedialog.cpp`: removing a
  palette color deleted the clicked button from inside its own
  `clicked()` handler (deleting a sender mid-signal is undefined in
  Qt). Widgets in the rebuilt row are now released with `deleteLater()`.
- **Potential infinite loop** — `pickTargetColor()` spun forever when a
  palette contained only duplicates of the current color; the
  duplicate-avoidance retry is now bounded.
- **Recording truncated on app quit** — closing fstl while recording
  destroyed the ffmpeg `QProcess` (killing the encoder and truncating
  the file). The dialog's new destructor calls `stop()` first, which
  finalizes the video.
- Two `-Wextra` warnings: an unused variable in `cliexport.cpp` after
  the `RenderStyle` refactor, and the parenthesized `dm_acts`
  declaration in `window.cpp`.

Reviewed and left as-is: the per-tick matrix inversion in
`absorbUserRotation` (trivial cost at 60 Hz); recording re-renders the
scene per captured frame on top of the live ticks (heavier but correct);
combining `--opacity` with transparent GIFs thresholds semi-transparent
pixels at the format's 1-bit alpha (an inherent GIF limitation).

### Testing

- `test/test_export.cpp` (new), built via `-DFSTL_BUILD_TESTS=ON`
  (`fstl_test_export` target in `CMakeLists.txt`): loads the bundled
  sphere into a real `Canvas`, asserts rotated frames are valid and
  differ, checks the PNG/GIF angle-sequence math (counts and spacing for
  loop and bounce), and encodes a GIF through the same calls the app uses,
  validating the output header. This harness is what exposed the two
  initialization crashes fixed above.

### Documentation

- `README.md` — new Usage section: GUI synopsis and `--foreground`; full
  command-line switch reference with defaults, accepted color formats,
  output numbering, exit codes, sizing rules, and headless notes; build
  docs updated for Qt 6/CMake 3.16/C++17 and `cpack` packaging.
- Built-in `--help` (in `src/cliexport.cpp`) extended with numbering, exit
  codes, and a pointer to `--foreground`.

---

### Known issues and concerns

- **Qt 6 verified at 6.4.2 only.** With `qt6-base-dev` installed, the
  Qt 6 build compiles warning-free and passes the GUI smoke test, the
  export test harness, and CLI PNG/GIF export checks. Newer Qt 6.x
  releases may introduce further deprecations. Note that now that Qt 6 is
  installed, a *fresh* CMake configure prefers it - the previously
  generated `.deb` was linked against Qt 5 and stays valid, but
  re-running `cpack` after a fresh configure produces a Qt 6-dependent
  package.
- **Transparent GIFs are larger and harder-edged.** Transparency disables
  delta encoding (every frame is stored whole), and GIF alpha is 1-bit, so
  anti-aliased model edges are thresholded at alpha 128 (a 120-frame
  640 px spin is ~1.1 MB). Opaque-background GIFs keep upstream delta
  encoding. PNG exports carry full 8-bit alpha and don't have this issue.
- **GUI vs CLI exports render differently by design.** GUI exports
  (`grabRotatedFrame`) capture the canvas as shown — current draw mode,
  window size, gradient backdrop; CLI exports use the offscreen renderer
  with flat/transparent background, the meshlight shader, and model-fit
  sizing. Same model, intentionally different pipelines.
- **Headless GL.** Without a display fstl falls back to Qt's `offscreen`
  platform, which can only create a GL context where EGL is available;
  otherwise it errors cleanly and `xvfb-run` is required. True
  software-GL fallback was not implemented.
- **Initial-load guard removed.** Opening a file that is *actively being
  written* (outside autoreload) may now read a partial file and report a
  bad STL, where it previously waited for the size to settle. Judged worth
  the 100 ms saved on every normal open; reload paths keep the guard.
- **Detached GUI keeps the terminal's stdout/stderr.** After the fork the
  prompt returns, but later Qt warnings still print into that terminal.
  Redirecting to `/dev/null` would hide real errors, so output was left
  attached.
- **Files starting with `-`.** The GUI's flag filtering means a file
  named e.g. `-model.stl` must be opened as `./-model.stl`.
- **Wayland popup warning (environment, not fstl).** On Wayland with
  Qt 6.4.x, opening nested menus can print `qt.qpa.wayland: setGrabPopup
  called with a parent ... which does not match the current topmost
  grabbing popup`. This is a known QtWayland grab-handling defect in the
  platform plugin (improved in Qt 6.5+); the plugin works around it itself
  and menus function normally. Not suppressed by fstl on purpose; users
  can launch with `QT_LOGGING_RULES="qt.qpa.wayland.warning=false"` or
  `QT_QPA_PLATFORM=xcb` if the message is unwanted.
- **Pre-existing, not addressed:** the benign `QSocketNotifier` warning at
  startup (appears on this system in Qt 5.15 sessions); the GIF angle
  sequencer accumulates floats, so step sizes that don't
  divide the range exactly can produce one frame more/less than the ideal
  count; CLI stdin input is buffered fully in memory before parsing.
- **Vendored `gif.h` divergence.** The transparency patch means future
  upstream updates of gif.h need the (clearly marked) patches re-applied.
