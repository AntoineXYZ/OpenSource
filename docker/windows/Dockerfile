FROM debian:13-slim AS toolchain

ARG DEBIAN_FRONTEND=noninteractive
ARG MXE_COMMIT=a5e9fac6b46ef9d96b78094aaf4e737a2b09d848
ARG MXE_OPENAL_COMMIT=6e368a5537941472b1940d575be8e11f4a380bfc
ARG MXE_TARGET=x86_64-w64-mingw32.shared

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        autoconf \
        automake \
        autopoint \
        bash \
        bison \
        bzip2 \
        ca-certificates \
        cmake \
        flex \
        g++ \
        g++-multilib \
        gettext \
        git \
        gperf \
        intltool \
        libc6-dev-i386 \
        libclang-dev \
        libgdk-pixbuf-2.0-dev \
        libgl-dev \
        libltdl-dev \
        libpcre2-dev \
        libssl-dev \
        libtool-bin \
        libxml-parser-perl \
        lzip \
        make \
        ninja-build \
        openssl \
        p7zip-full \
        patch \
        perl \
        python3 \
        python3-mako \
        python3-packaging \
        python3-pkg-resources \
        python3-setuptools \
        python3-yaml \
        python-is-python3 \
        ruby \
        sed \
        sqlite3 \
        unzip \
        wget \
        xz-utils \
    && rm -rf /var/lib/apt/lists/*

# OpenAL 1.25 requires newer C++ ranges support than MXE's pinned GCC 11.
RUN git clone https://github.com/mxe/mxe.git /opt/mxe \
    && git -C /opt/mxe checkout "${MXE_COMMIT}" \
    && git -C /opt/mxe checkout "${MXE_OPENAL_COMMIT}" -- src/openal.mk

WORKDIR /opt/mxe
RUN make -j"$(nproc)" \
        MXE_TARGETS="${MXE_TARGET}" \
        qtbase \
        sdl2 \
        openal

RUN test -s /opt/mxe/usr/x86_64-pc-linux-gnu/bin/ninja \
    && test -s "/opt/mxe/usr/${MXE_TARGET}/share/cmake/mxe-conf.cmake" \
    && test -s "/opt/mxe/usr/bin/${MXE_TARGET}-g++" \
    && /opt/mxe/usr/x86_64-pc-linux-gnu/bin/ninja --version

ENV PATH="/opt/mxe/usr/bin:${PATH}"
ENV MXE_TARGET="${MXE_TARGET}"

WORKDIR /src
CMD ["/bin/bash"]
