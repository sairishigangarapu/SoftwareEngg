# UATC - cmake-free build (for environments without cmake)
# Primary build is via CMakeLists.txt when cmake is available.
# This Makefile provides the same targets for quick local builds.

CXX ?= g++
CXXFLAGS = -std=c++17 -O2 -DNDEBUG -Iinclude
PC_CXXFLAGS = -std=c++17 -O2 -DNDEBUG -Ipcomp/include -Iinclude

# ---- Core library sources (Rishi's core + Chakshu's archive + filesystem) ----
CORE_SRCS = \
	src/core/block_stats.cpp \
	src/core/block_slicer.cpp \
	src/core/baseline_entropy_coder.cpp \
	src/archive/archive_binary.cpp \
	src/archive/archive_reader.cpp \
	src/archive/archive_writer.cpp \
	src/archive/block.cpp \
	src/archive/codec.cpp \
	src/archive/transforms.cpp \
	src/filesystem/file_enumerator.cpp \
	src/filesystem/file_reader.cpp

CORE_OBJS = $(CORE_SRCS:.cpp=.o)

# ---- Test executables ----
CORE_TESTS = \
	tests/core/trivial_test \
	tests/core/block_slice_test \
	tests/core/transform_test \
	tests/core/codec_test \
	tests/core/pipeline_test

CHAKSHU_TESTS = \
	tests/filesystem_test \
	tests/archive_binary_test \
	tests/block_test \
	tests/archive_writer_test \
	tests/archive_reader_test \
	tests/transform_codec_test \
	tests/mixed_transform_test

ALL_TESTS = $(CORE_TESTS) $(CHAKSHU_TESTS)

# ---- pcomp benchmark ----
PC_BENCH = pcomp/build/pc_bench

# ---- Default target ----
.PHONY: all bench test clean core-tests chakshu-tests

all: $(ALL_TESTS) $(PC_BENCH)

# ---- Core library build ----
build/libuatc_core.a: $(CORE_OBJS)
	mkdir -p build
	ar rcs $@ $(CORE_OBJS)

$(CORE_OBJS): %.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ---- Core test build rules ----
tests/core/trivial_test: tests/core/trivial_test.cpp build/libuatc_core.a
	$(CXX) $(CXXFLAGS) $< $(CORE_OBJS) -o $@

tests/core/block_slice_test: tests/core/block_slice_test.cpp build/libuatc_core.a
	$(CXX) $(CXXFLAGS) $< $(CORE_OBJS) -o $@

tests/core/transform_test: tests/core/transform_test.cpp build/libuatc_core.a
	$(CXX) $(CXXFLAGS) $< $(CORE_OBJS) -o $@

tests/core/codec_test: tests/core/codec_test.cpp build/libuatc_core.a
	$(CXX) $(CXXFLAGS) $< $(CORE_OBJS) -o $@

tests/core/pipeline_test: tests/core/pipeline_test.cpp build/libuatc_core.a
	$(CXX) $(CXXFLAGS) $< $(CORE_OBJS) -o $@

# ---- Chakshu's tests ----
tests/filesystem_test: tests/filesystem_test.cpp build/libuatc_core.a
	$(CXX) $(CXXFLAGS) $< $(CORE_OBJS) -o $@

tests/archive_binary_test: tests/archive_binary_test.cpp build/libuatc_core.a
	$(CXX) $(CXXFLAGS) $< $(CORE_OBJS) -o $@

tests/block_test: tests/block_test.cpp build/libuatc_core.a
	$(CXX) $(CXXFLAGS) $< $(CORE_OBJS) -o $@

tests/archive_writer_test: tests/archive_writer_test.cpp build/libuatc_core.a
	$(CXX) $(CXXFLAGS) $< $(CORE_OBJS) -o $@

tests/archive_reader_test: tests/archive_reader_test.cpp build/libuatc_core.a
	$(CXX) $(CXXFLAGS) $< $(CORE_OBJS) -o $@

tests/transform_codec_test: tests/transform_codec_test.cpp build/libuatc_core.a
	$(CXX) $(CXXFLAGS) $< $(CORE_OBJS) -o $@

tests/mixed_transform_test: tests/mixed_transform_test.cpp build/libuatc_core.a
	$(CXX) $(CXXFLAGS) $< $(CORE_OBJS) -o $@

# ---- Run all tests ----
test: $(ALL_TESTS)
	@for t in $(ALL_TESTS); do \
		echo "Running $$t..."; \
		./$$t || exit 1; \
	done
	@echo "ALL TESTS PASSED"

# ---- pcomp benchmark ----
$(PC_BENCH): pcomp/src/block_loop.cpp pcomp/bench/bench_main.cpp
	mkdir -p pcomp/build
	$(CXX) $(PC_CXXFLAGS) pcomp/src/block_loop.cpp pcomp/bench/bench_main.cpp -o $@

pc_bench: $(PC_BENCH)

bench: $(PC_BENCH)
	@echo "pc_bench built. Run: make run-bench"

run-bench: $(PC_BENCH)
	python3 pcomp/scripts/make_samples.py
	./$(PC_BENCH) --root pcomp/datasets/samples --out pcomp/results/bench.jsonl \
		--block-size 1048576 --reps 2 --workers 1,2,4,max

# ---- Cleanup ----
clean:
	rm -f $(CORE_OBJS) build/libuatc_core.a
	rm -f $(ALL_TESTS)
	rm -f $(PC_BENCH)
