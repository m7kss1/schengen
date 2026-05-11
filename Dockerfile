# syntax=docker/dockerfile:1

FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
ENV RUSTUP_HOME=/usr/local/rustup
ENV CARGO_HOME=/usr/local/cargo
ENV PATH=/usr/local/cargo/bin:$PATH

SHELL ["/bin/bash", "-lc"]

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    curl \
    git \
    ninja-build \
    pkg-config \
    python3 \
    python3-pip \
    libssl-dev \
    libcurl4-openssl-dev \
    libre2-dev \
    zlib1g-dev \
    libboost-program-options-dev \
    protobuf-compiler \
    libprotobuf-dev \
    libclang-dev \
    && rm -rf /var/lib/apt/lists/*

RUN pip3 install --no-cache-dir "cmake>=3.25,<4.0"

RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs \
    | sh -s -- -y --default-toolchain 1.91.0 --no-modify-path \
    && rustup toolchain install 1.90

WORKDIR /src
COPY . .

RUN cmake -S . -B /build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DENABLE_PARQUET=ON \
      -DENABLE_ORC=ON \
      -DENABLE_LANCE=ON \
      -DENABLE_VORTEX=ON

RUN --mount=type=cache,target=/usr/local/cargo/registry \
    --mount=type=cache,target=/usr/local/cargo/git \
    --mount=type=cache,target=/build/cargo \
    cmake --build /build --target schengen_main -j"$(nproc)" \
    && strip /build/schengen_main \
    && cp /build/schengen_main /schengen_main

FROM ubuntu:22.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    libssl3 \
    libcurl4 \
    zlib1g \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /schengen_main /usr/local/bin/schengen

ENTRYPOINT ["/usr/local/bin/schengen"]