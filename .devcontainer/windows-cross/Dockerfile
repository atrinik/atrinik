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

ARG MXE_REF=master
ARG MXE_BUILD_JOBS=2
RUN git clone https://github.com/mxe/mxe.git /opt/mxe \
    && git -C /opt/mxe checkout "${MXE_REF}"

COPY .devcontainer/windows-cross/curl-openssl.patch /tmp/curl-openssl.patch
RUN git -C /opt/mxe apply /tmp/curl-openssl.patch \
    && chown -R vscode:vscode /opt/mxe

USER vscode
RUN make -C /opt/mxe --jobs="${MXE_BUILD_JOBS}" JOBS="${MXE_BUILD_JOBS}" \
        MXE_TARGETS=x86_64-w64-mingw32.shared \
        openssl
RUN make -C /opt/mxe --jobs="${MXE_BUILD_JOBS}" JOBS="${MXE_BUILD_JOBS}" \
        MXE_TARGETS=x86_64-w64-mingw32.shared \
        cc cmake curl openssl zlib libxml2 sdl sdl_image sdl_mixer sdl_ttf
RUN test "$(/opt/mxe/usr/x86_64-w64-mingw32.shared/bin/curl-config --ssl-backends)" \
        = "OpenSSL"
USER root

ENV MXE_ROOT=/opt/mxe \
    MXE_TARGET=x86_64-w64-mingw32.shared \
    MXE_TOOLCHAIN_FILE=/opt/mxe/usr/x86_64-w64-mingw32.shared/share/cmake/mxe-conf.cmake \
    MXE_RUNTIME_DIR=/opt/mxe/usr/x86_64-w64-mingw32.shared/bin \
    PATH=/opt/mxe/usr/bin:${PATH}
