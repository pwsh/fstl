# CMake toolchain for cross-compiling fstl to Windows x86_64 with MinGW.
# Expects the official Qt MinGW package at /opt/qt/<version>/mingw_64 and
# the same-version official Linux Qt at /opt/qt/<version>/gcc_64 for
# moc/rcc/uic (pass -DQT_HOST_PATH=/opt/qt/<version>/gcc_64).
# The version is taken from the QT_VERSION environment variable (set by the
# cross/Dockerfile) and defaults to 6.10.3.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

if(DEFINED ENV{QT_VERSION})
    set(FSTL_CROSS_QT_VERSION $ENV{QT_VERSION})
else()
    set(FSTL_CROSS_QT_VERSION 6.10.3)
endif()

set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc-posix)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++-posix)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

set(CMAKE_FIND_ROOT_PATH /opt/qt/${FSTL_CROSS_QT_VERSION}/mingw_64 /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Static MinGW runtime so the bundle only needs the Qt DLLs
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static")

# Run the (same-version) native host Qt tools for autogen; the target
# package only ships Windows .exe tools
set(CMAKE_AUTOMOC_EXECUTABLE /opt/qt/${FSTL_CROSS_QT_VERSION}/gcc_64/libexec/moc)
set(CMAKE_AUTORCC_EXECUTABLE /opt/qt/${FSTL_CROSS_QT_VERSION}/gcc_64/libexec/rcc)
set(CMAKE_AUTOUIC_EXECUTABLE /opt/qt/${FSTL_CROSS_QT_VERSION}/gcc_64/libexec/uic)

# The Linux rcc defaults to zstd, which the official Qt MinGW build does
# not export; use zlib so the generated code links against the target Qt
set(CMAKE_AUTORCC_OPTIONS "--compress-algo;zlib")
