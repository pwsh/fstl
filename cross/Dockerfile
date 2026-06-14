# Cross-compiles fstl for Windows (x86_64) from Linux.
# Target Qt: official Qt 6.4.2 MinGW binaries (fetched with aqtinstall).
# Host tools: Ubuntu 24.04's Qt 6.4.2 (same version, as Qt cross builds require).
FROM ubuntu:24.04

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        g++-mingw-w64-x86-64-posix \
        cmake make \
        qt6-base-dev qt6-base-dev-tools \
        python3-pip python3-venv \
        zip ca-certificates \
    && rm -rf /var/lib/apt/lists/*

RUN python3 -m pip install --break-system-packages aqtinstall

# Official Qt for Windows/MinGW; kept in its own layer so rebuilds are cheap
RUN aqt install-qt linux desktop 6.4.2 --help > /dev/null 2>&1 || true
RUN aqt install-qt windows desktop 6.4.2 win64_mingw -O /opt/qt

# Wine + Xvfb for headless smoke-testing of the built .exe (separate layer
# so the large Qt download above stays cached when this changes)
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        wine wine64 xvfb \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
