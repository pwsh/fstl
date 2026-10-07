# Cross-compiles fstl for Windows (x86_64) from Linux, and builds the
# portable Linux release (binary + .deb) against Ubuntu 24.04's Qt 6.4.
# Target Qt: official Qt MinGW binaries (fetched with aqtinstall).
# Host tools: the same-version official Linux Qt (Qt cross builds require
# moc/rcc/uic of exactly the target version).
FROM ubuntu:24.04

ARG QT_VERSION=6.10.3
ENV QT_VERSION=${QT_VERSION}

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        g++-mingw-w64-x86-64-posix \
        build-essential cmake make \
        qt6-base-dev qt6-base-dev-tools libgl-dev \
        libglib2.0-0 libxkbcommon0 libfontconfig1 libdbus-1-3 \
        python3-pip python3-venv \
        zip gzip file dpkg-dev ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# aqtinstall 3.3.0 (newest as of 2026-10) resolves Qt up to 6.10.x for
# windows desktop; 6.11+ uses a repository layout it does not understand yet.
RUN python3 -m pip install --break-system-packages "aqtinstall==3.3.0"

# Official Qt: Windows/MinGW target plus the same-version Linux host tools;
# kept in their own layers so rebuilds are cheap
RUN aqt install-qt linux desktop ${QT_VERSION} linux_gcc_64 -O /opt/qt
RUN aqt install-qt windows desktop ${QT_VERSION} win64_mingw -O /opt/qt

# Wine + Xvfb for headless smoke-testing of the built .exe (separate layer
# so the large Qt download above stays cached when this changes)
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        wine wine64 xvfb \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
