#
# Top-level Makefile — convenience wrapper around CMake.
#
# dosiz runs on the emu88 backend, which is OWNED BY THE qxDOS REPO and read
# from a sibling checkout (../qxDOS/emu88) rather than copied in-tree. There is
# no DOSBox dependency: emu88 is self-contained, so the build is just CMake over
# emu88 + dosiz's CLI. Real build rules live in src/CMakeLists.txt.
#
# Requires github.com/avwohl/qxDOS checked out next to this repo. If it lives
# elsewhere: cmake -S src -B build -DEMU88_DIR=/path/to/qxDOS/emu88
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
