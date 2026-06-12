# CMake toolchain for cross-compiling fstl to Windows x86_64 with MinGW.
# Expects the official Qt MinGW package at /opt/qt/6.4.2/mingw_64 and a
# same-version host Qt for moc/rcc (pass -DQT_HOST_PATH=/usr).
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc-posix)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++-posix)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

set(CMAKE_FIND_ROOT_PATH /opt/qt/6.4.2/mingw_64 /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Static MinGW runtime so the bundle only needs the Qt DLLs
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static")

# Run the (same-version) native host Qt tools for autogen; the target
# package only ships Windows .exe tools
set(CMAKE_AUTOMOC_EXECUTABLE /usr/lib/qt6/libexec/moc)
set(CMAKE_AUTORCC_EXECUTABLE /usr/lib/qt6/libexec/rcc)
set(CMAKE_AUTOUIC_EXECUTABLE /usr/lib/qt6/libexec/uic)

# Ubuntu's rcc defaults to zstd, which the official Qt MinGW build does
# not export; use zlib so the generated code links against the target Qt
set(CMAKE_AUTORCC_OPTIONS "--compress-algo;zlib")
