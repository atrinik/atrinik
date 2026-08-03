FROM mcr.microsoft.com/devcontainers/base:bookworm

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        autoconf \
        automake \
        autopoint \
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
        mingw-w64-tools \
        ninja-build \
        openssl \
        p7zip-full \
        patch \
        perl \
        python-is-python3 \
        python3 \
        python3-mako \
        python3-packaging \
        python3-pkg-resources \
        python3-setuptools \
        ruby \
        sqlite3 \
        unzip \
        wget \
        xz-utils \
        zip \
    && rm -rf /var/lib/apt/lists/*

ARG MXE_REF=0b88e56678ee5b2afe9c294e53a23c9d8ac30a15
ARG MXE_BUILD_JOBS=2
ARG MINIUPNPC_COMMIT=bf4215a7574f88aa55859db9db00e3ae58cf42d6
ARG PYTHON_VERSION=3.14.6
ARG PYTHON_EMBED_SHA256=df901e84a896ff1ee720ad03377e0c8d8c2244fda79808aeeaff6316df1cb75c
ARG PYTHON_SOURCE_SHA256=143b1dddefaec3bd2e21e3b839b34a2b7fb9842272883c576420d605e9f30c63
RUN git clone https://github.com/mxe/mxe.git /opt/mxe \
    && git -C /opt/mxe checkout "${MXE_REF}"

COPY .devcontainer/windows-cross/curl-openssl.patch /tmp/curl-openssl.patch
RUN git -C /opt/mxe apply /tmp/curl-openssl.patch \
    && chown -R vscode:vscode /opt/mxe

ENV MXE_ROOT=/opt/mxe \
    MXE_TARGET=x86_64-w64-mingw32.shared \
    MXE_PREFIX=/opt/mxe/usr/x86_64-w64-mingw32.shared \
    MXE_TOOLCHAIN_FILE=/opt/mxe/usr/x86_64-w64-mingw32.shared/share/cmake/mxe-conf.cmake \
    MXE_RUNTIME_DIR=/opt/mxe/usr/x86_64-w64-mingw32.shared/bin \
    ATRINIK_WINDOWS_PYTHON_INCLUDE_DIR=/opt/mxe/usr/x86_64-w64-mingw32.shared/include/python3.14 \
    ATRINIK_WINDOWS_PYTHON_LIBRARY=/opt/mxe/usr/x86_64-w64-mingw32.shared/lib/libpython314.dll.a \
    ATRINIK_WINDOWS_PYTHON_RUNTIME_DIR=/opt/mxe/python-runtime \
    PATH=/opt/mxe/usr/bin:${PATH}

USER vscode
RUN make -C /opt/mxe --jobs="${MXE_BUILD_JOBS}" JOBS="${MXE_BUILD_JOBS}" \
        MXE_TARGETS=x86_64-w64-mingw32.shared \
        openssl
RUN make -C /opt/mxe --jobs="${MXE_BUILD_JOBS}" JOBS="${MXE_BUILD_JOBS}" \
        MXE_TARGETS=x86_64-w64-mingw32.shared \
        cc cmake curl openssl zlib libxml2 sdl sdl_image sdl_mixer sdl_ttf
RUN test "$(/opt/mxe/usr/x86_64-w64-mingw32.shared/bin/curl-config --ssl-backends)" \
        = "OpenSSL"

RUN git clone https://github.com/miniupnp/miniupnp.git /tmp/miniupnp \
    && git -C /tmp/miniupnp checkout "${MINIUPNPC_COMMIT}" \
    && x86_64-w64-mingw32.shared-cmake \
        -S /tmp/miniupnp/miniupnpc \
        -B /tmp/miniupnp-build \
        -DUPNPC_BUILD_STATIC=ON \
        -DUPNPC_BUILD_SHARED=OFF \
        -DUPNPC_BUILD_TESTS=OFF \
    && cmake --build /tmp/miniupnp-build --parallel "${MXE_BUILD_JOBS}" \
    && cmake --install /tmp/miniupnp-build \
    && rm -rf /tmp/miniupnp /tmp/miniupnp-build

COPY --chown=vscode:vscode --chmod=0755 dependencies /tmp/atrinik-dependencies
COPY --chown=vscode:vscode .devcontainer/windows-cross/libpcpnatpmp-mingw.patch \
    /tmp/.devcontainer/windows-cross/libpcpnatpmp-mingw.patch
RUN /tmp/atrinik-dependencies/build-libpcpnatpmp.sh mingw "${MXE_BUILD_JOBS}"

RUN mkdir -p "${ATRINIK_WINDOWS_PYTHON_RUNTIME_DIR}" /tmp/python-sdk \
    && wget -q \
        "https://www.python.org/ftp/python/${PYTHON_VERSION}/python-${PYTHON_VERSION}-embed-amd64.zip" \
        -O /tmp/python-sdk/python-embed.zip \
    && echo "${PYTHON_EMBED_SHA256}  /tmp/python-sdk/python-embed.zip" \
        | sha256sum -c - \
    && wget -q \
        "https://www.python.org/ftp/python/${PYTHON_VERSION}/Python-${PYTHON_VERSION}.tar.xz" \
        -O /tmp/python-sdk/python-source.tar.xz \
    && echo "${PYTHON_SOURCE_SHA256}  /tmp/python-sdk/python-source.tar.xz" \
        | sha256sum -c - \
    && unzip -q /tmp/python-sdk/python-embed.zip \
        -d "${ATRINIK_WINDOWS_PYTHON_RUNTIME_DIR}" \
    && tar -xf /tmp/python-sdk/python-source.tar.xz -C /tmp/python-sdk \
    && mkdir -p "${ATRINIK_WINDOWS_PYTHON_INCLUDE_DIR}" \
    && cp -R "/tmp/python-sdk/Python-${PYTHON_VERSION}/Include/." \
        "${ATRINIK_WINDOWS_PYTHON_INCLUDE_DIR}/" \
    && cp "/tmp/python-sdk/Python-${PYTHON_VERSION}/PC/pyconfig.h" \
        "${ATRINIK_WINDOWS_PYTHON_INCLUDE_DIR}/pyconfig.h" \
    && cd /tmp/python-sdk \
    && gendef "${ATRINIK_WINDOWS_PYTHON_RUNTIME_DIR}/python314.dll" \
    && x86_64-w64-mingw32.shared-dlltool \
        -d python314.def \
        -D python314.dll \
        -l "${ATRINIK_WINDOWS_PYTHON_LIBRARY}" \
    && rm -rf /tmp/python-sdk

USER root

# Native build dependencies used by build-windows.sh to run the Linux world
# maker before cross-compiling and packaging the Windows server.
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        libcurl4-openssl-dev \
        libgd-dev \
        libminiupnpc-dev \
        python3-dev \
        zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

RUN /tmp/atrinik-dependencies/build-libpcpnatpmp.sh native "${MXE_BUILD_JOBS}"
