FROM ubuntu:26.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        check \
        cmake \
        cproto \
        flex \
        gdb \
        git \
        libcurl4-openssl-dev \
        libgd-dev \
        libreadline-dev \
        libsubunit-dev \
        libsdl-image1.2-dev \
        libsdl-mixer1.2-dev \
        libsdl-ttf2.0-dev \
        libsdl1.2-dev \
        libssl-dev \
        libx11-dev \
        libxml2-dev \
        libxml2-utils \
        nodejs \
        npm \
        libxmu-dev \
        pkgconf \
        python3-dev \
        timidity \
        valgrind \
        zlib1g-dev \
        alsa-utils \
        libasound2-plugins \
        pulseaudio-utils \
    && rm -rf /var/lib/apt/lists/*

COPY .devcontainer/asound.conf /etc/asound.conf
