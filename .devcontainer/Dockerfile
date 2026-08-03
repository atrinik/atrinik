# syntax=docker/dockerfile:1

FROM ubuntu:26.04

ENV DEBIAN_FRONTEND=noninteractive

RUN rm -f /etc/apt/apt.conf.d/docker-clean

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt/lists,sharing=locked \
    apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        check \
        ccache \
        clang-format \
        clang-tidy \
        clangd \
        cmake \
        cppcheck \
        cproto \
        fd-find \
        file \
        flex \
        fzf \
        gdb \
        git \
        jq \
        libcurl4-openssl-dev \
        libgd-dev \
        libminiupnpc-dev \
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
        ninja-build \
        nodejs \
        npm \
        patch \
        libxmu-dev \
        pkgconf \
        python3-dev \
        python3-pip \
        python3-venv \
        ripgrep \
        shellcheck \
        shfmt \
        sudo \
        timidity \
        tree \
        uncrustify \
        valgrind \
        zlib1g-dev \
        iproute2 \
        alsa-utils \
        libasound2-plugins \
        pulseaudio-utils

ARG PCPNATPMP_COMMIT=866d283da99f5e98eecff702a8df63e2ae57ffca
RUN git clone https://github.com/libpcpnatpmp/libpcpnatpmp.git /tmp/libpcpnatpmp \
    && git -C /tmp/libpcpnatpmp checkout "${PCPNATPMP_COMMIT}" \
    && cmake -S /tmp/libpcpnatpmp -B /tmp/libpcpnatpmp/build \
        -DBUILD_SHARED_LIBS=OFF \
        -DBUILD_CLI_CLIENT=OFF \
        -DBUILD_SERVER=OFF \
        -DBUILD_TESTS=OFF \
    && cmake --build /tmp/libpcpnatpmp/build --parallel \
    && cmake --install /tmp/libpcpnatpmp/build \
    && rm -rf /tmp/libpcpnatpmp

# Ubuntu packages fd as "fdfind" to avoid a naming conflict. Provide the
# conventional command name expected by editor integrations and CLI tooling.
RUN ln -s /usr/bin/fdfind /usr/local/bin/fd

COPY .devcontainer/asound.conf /etc/asound.conf

# Ubuntu 26.04 already provides this non-root account. Dev Containers updates
# its UID/GID to match the owner of the bind-mounted repository.
ARG USERNAME=ubuntu
RUN install -d -o "${USERNAME}" -g "${USERNAME}" "/home/${USERNAME}/.codex" \
    && printf '%s ALL=(ALL) NOPASSWD:ALL\n' "${USERNAME}" \
        > "/etc/sudoers.d/${USERNAME}" \
    && chmod 0440 "/etc/sudoers.d/${USERNAME}"
