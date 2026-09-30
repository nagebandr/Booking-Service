# syntax=docker/dockerfile:1

########################################
# Stage 1: Build Environment
########################################
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    git \
    pkg-config \
    libboost-all-dev \
    libpqxx-dev \
    libpq-dev \
    nlohmann-json3-dev \
    libgtest-dev \
    libbenchmark-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy CMake files first for caching
COPY CMakeLists.txt ./
COPY tests/CMakeLists.txt tests/
COPY benchmarks/CMakeLists.txt benchmarks/
COPY include/ include/
COPY src/ src/
COPY tests/ tests/
COPY benchmarks/ benchmarks/

# Configure and build
RUN cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=ON \
    -DBUILD_BENCHMARKS=ON

RUN cmake --build build --target booking_server booking_tests booking_benchmarks -j$(nproc)

########################################
# Stage 2: Runtime Image
########################################
FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    libpqxx-7.8* \
    libpq5 \
    ca-certificates \
    curl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /usr/lib/x86_64-linux-gnu/libbenchmark* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /app/build/booking_server /usr/local/bin/booking_server
COPY --from=builder /app/build/tests/booking_tests /usr/local/bin/booking_tests
COPY --from=builder /app/build/benchmarks/booking_benchmarks /usr/local/bin/booking_benchmarks

EXPOSE 8080

ENTRYPOINT ["/usr/local/bin/booking_server"]
