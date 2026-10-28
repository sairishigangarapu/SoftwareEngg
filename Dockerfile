# UATC - reproducible build and test (TC-NFR-06).
#
#   docker build -t uatc .
#   docker run --rm uatc                  # runs the full test suite
#   docker run --rm uatc scripts/demo.sh  # dry run inside the image
#
FROM ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake python3 gzip ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /uatc
COPY . .

# The lab machines have no cmake, so the Makefile is the primary build; in
# the image cmake exists, and building through CMakeLists.txt exercises the
# CTest targets as well.
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -j"$(nproc)" \
    && ctest --test-dir build --output-on-failure

CMD ["bash", "-lc", "ctest --test-dir build --output-on-failure"]
