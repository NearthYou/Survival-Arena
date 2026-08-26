# syntax=docker/dockerfile:1.7

ARG UBUNTU_VERSION=24.04

FROM ubuntu:${UBUNTU_VERSION} AS builder

ARG VCPKG_COMMIT=127402f1c75bb3d5ff6bce04b285faa4930a5aca

ENV DEBIAN_FRONTEND=noninteractive
ENV VCPKG_ROOT=/opt/vcpkg

RUN apt-get update \
    && apt-get install --yes --no-install-recommends \
        build-essential \
        ca-certificates \
        cmake \
        curl \
        git \
        ninja-build \
        pkg-config \
        tar \
        unzip \
        zip \
    && rm -rf /var/lib/apt/lists/*

RUN git init "${VCPKG_ROOT}" \
    && git -C "${VCPKG_ROOT}" remote add origin \
        https://github.com/microsoft/vcpkg.git \
    && git -C "${VCPKG_ROOT}" fetch --depth 1 origin "${VCPKG_COMMIT}" \
    && git -C "${VCPKG_ROOT}" checkout --detach FETCH_HEAD \
    && "${VCPKG_ROOT}/bootstrap-vcpkg.sh" -disableMetrics

COPY vcpkg.json /tmp/dxa-manifest/vcpkg.json

RUN "${VCPKG_ROOT}/vcpkg" install \
        --x-manifest-root=/tmp/dxa-manifest \
        --x-install-root=/opt/vcpkg_installed \
        --triplet=x64-linux \
        --clean-after-build

WORKDIR /src
COPY . .

RUN cmake -S . -B out/build/server -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/dxa \
        -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" \
        -DVCPKG_INSTALLED_DIR=/opt/vcpkg_installed \
        -DVCPKG_TARGET_TRIPLET=x64-linux \
        -DDXA_BUILD_TESTS=OFF \
        -DDXA_WARNINGS_AS_ERRORS=ON \
    && cmake --build out/build/server \
        --target dxa_lobby_server dxa_game_server \
    && cmake --install out/build/server --component Server

FROM ubuntu:${UBUNTU_VERSION} AS runtime

ARG VCS_REF=unknown

LABEL org.opencontainers.image.title="DX11 Survival Arena servers" \
      org.opencontainers.image.description="Lobby and authoritative game server runtime" \
      org.opencontainers.image.source="https://github.com/NearthYou/Survival-Arena" \
      org.opencontainers.image.revision="${VCS_REF}" \
      org.opencontainers.image.licenses="MIT"

RUN apt-get update \
    && apt-get install --yes --no-install-recommends \
        libgcc-s1 \
        libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder --chown=10001:10001 \
    /opt/dxa/bin/dxa_lobby_server \
    /opt/dxa/bin/dxa_game_server \
    /usr/local/bin/
COPY --chmod=0555 deploy/docker/socket-healthcheck.sh \
    /usr/local/bin/dxa-socket-healthcheck

USER 10001:10001
WORKDIR /app

EXPOSE 7000/tcp 7001/tcp 7100/tcp 7101/udp 7200/tcp 7201/udp

STOPSIGNAL SIGTERM
CMD ["/usr/local/bin/dxa_lobby_server"]
