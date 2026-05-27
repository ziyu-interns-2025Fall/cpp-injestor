FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    libpoppler-cpp-dev \
    libhiredis-dev \
    nlohmann-json3-dev \
    libcurl4-openssl-dev \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY CMakeLists.txt .
COPY src/ src/
COPY tests/ tests/

RUN mkdir build && cd build && cmake .. && make -j$(nproc) ingestor

ENTRYPOINT ["./build/ingestor", "--workers", "4"]
