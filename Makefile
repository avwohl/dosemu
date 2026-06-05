#
# Top-level Makefile — convenience wrapper around CMake.
#
# dosiz runs on the in-tree emu88 backend (../emu88, vendored). There is no
# longer a DOSBox dependency: emu88 is self-contained, so the build is just
# CMake over emu88 + dosiz's CLI. Real build rules live in src/CMakeLists.txt.
#

BUILD_DIR ?= build
JOBS      ?= $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

.PHONY: all build configure clean distclean test

all: build

configure:
	cmake -S src -B $(BUILD_DIR)

build: configure
	cmake --build $(BUILD_DIR) -j$(JOBS)

clean:
	rm -rf $(BUILD_DIR)

distclean: clean

test: build
	cd $(BUILD_DIR) && ctest --output-on-failure
