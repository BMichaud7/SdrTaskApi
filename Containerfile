# ════════════════════════════════════════════════════════════════════════
#  Containerfile — SdrTaskApi (library + unit tests)
#  Base: Ubuntu 24.04
#  All dependencies (nlohmann/json, spdlog, googletest) are fetched
#  automatically by CMake FetchContent — no extra packages needed.
#
#  Run unit tests:
#    podman build --target test .
# ════════════════════════════════════════════════════════════════════════

# ── Stage 1: Builder ──────────────────────────────────────────────────────
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        pkg-config \
        git \
        ca-certificates \
        libfmt-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace/SdrTaskApi
COPY CMakeLists.txt .
COPY include/       include/
COPY src/           src/
COPY tests/         tests/

RUN cmake -B build \
        -S . \
        -DCMAKE_BUILD_TYPE=Release \
        -DFETCHCONTENT_QUIET=OFF \
    && cmake --build build --parallel "$(nproc)"


# ── Stage 2: Test runner ──────────────────────────────────────────────────
# podman build --target test .
FROM builder AS test
RUN ctest --test-dir build --output-on-failure -V
