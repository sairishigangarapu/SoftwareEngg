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
	src/core/cost_estimator.cpp \
	src/core/delta_transform.cpp \
	src/core/xor_transform.cpp \
	src/core/byte_shuffle_transform.cpp \
	src/core/rle_transform.cpp \
	src/core/transform_factory.cpp \
	src/core/compressibility_probe.cpp \
	src/core/transform_selector.cpp \
	src/core/compressor.cpp \
	src/core/error.cpp \
	src/core/planner.cpp \
	src/archive/archive_binary.cpp \
	src/archive/archive_reader.cpp \
	src/archive/archive_writer.cpp \
	src/archive/block.cpp \
	src/archive/codec.cpp \
	src/archive/transforms.cpp \
	src/archive/archive_inspector.cpp \
	src/archive/archive_verifier.cpp \
	src/filesystem/file_enumerator.cpp \
	src/filesystem/file_reader.cpp

CORE_OBJS = $(CORE_SRCS:.cpp=.o)

# ---- Test executables ----
CORE_TESTS = \
	tests/core/trivial_test \
	tests/core/block_slice_test \
	tests/core/transform_test \
	tests/core/codec_test \
	tests/core/pipeline_test \
	tests/core/transforms_extended_test \
	tests/core/probe_test \
	tests/core/selector_test \
	tests/core/archive_integration_test \
	tests/core/compressor_test \
	tests/core/planner_test

CHAKSHU_TESTS = \
	tests/filesystem_test \
	tests/archive_binary_test \
	tests/block_test \
	tests/archive_writer_test \
	tests/archive_reader_test \
	tests/transform_codec_test \
	tests/mixed_transform_test

MANASA_TESTS = \
	tests/archive_inspector_test \
	tests/archive_verifier_test \
	tests/integration_roundtrip_test

ALL_TESTS = $(CORE_TESTS) $(CHAKSHU_TESTS) $(MANASA_TESTS)

# ---- CLI executable (info / verify) ----
CLI_BIN = build/uatc_cli

# ---- pcomp benchmark ----
PC_BENCH = pcomp/build/pc_bench

# ---- Default target ----
.PHONY: all bench test clean cli pc_bench run-bench core-tests chakshu-tests manasa-tests

all: $(ALL_TESTS) $(PC_BENCH) $(CLI_BIN)

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

tests/core/transforms_extended_test: tests/core/transforms_extended_test.cpp build/libuatc_core.a
	$(CXX) $(CXXFLAGS) $< $(CORE_OBJS) -o $@

tests/core/probe_test: tests/core/probe_test.cpp build/libuatc_core.a
	$(CXX) $(CXXFLAGS) $< $(CORE_OBJS) -o $@

tests/core/selector_test: tests/core/selector_test.cpp build/libuatc_core.a
	$(CXX) $(CXXFLAGS) $< $(CORE_OBJS) -o $@

tests/core/archive_integration_test: tests/core/archive_integration_test.cpp build/libuatc_core.a
	$(CXX) $(CXXFLAGS) $< $(CORE_OBJS) -o $@

tests/core/compressor_test: tests/core/compressor_test.cpp build/libuatc_core.a
	$(CXX) $(CXXFLAGS) $< $(CORE_OBJS) -o $@

tests/core/planner_test: tests/core/planner_test.cpp build/libuatc_core.a
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

# ---- Manasa's tests (inspection, verification, round-trip) ----
tests/archive_inspector_test: tests/archive_inspector_test.cpp build/libuatc_core.a
	$(CXX) $(CXXFLAGS) $< $(CORE_OBJS) -o $@

tests/archive_verifier_test: tests/archive_verifier_test.cpp build/libuatc_core.a
	$(CXX) $(CXXFLAGS) $< $(CORE_OBJS) -o $@

tests/integration_roundtrip_test: tests/integration_roundtrip_test.cpp build/libuatc_core.a
	$(CXX) $(CXXFLAGS) $< $(CORE_OBJS) -o $@

# ---- CLI executable (info / verify) ----
$(CLI_BIN): src/cli/main.cpp build/libuatc_core.a
	mkdir -p build
	$(CXX) $(CXXFLAGS) $< $(CORE_OBJS) -o $@

cli: $(CLI_BIN)

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
	rm -f $(CLI_BIN)
