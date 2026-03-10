SHELL := /usr/bin/env bash

.SHELLFLAGS := -eu -o pipefail -c
.DEFAULT_GOAL := help

PROFILE   := $(or $(profile),default)
BUILDTYPE := $(or $(buildtype),Debug)
CARTRIDGE := $(or $(CARTRIDGE),./cartridge)
NCPUS     := $(shell sysctl -n hw.ncpu 2>/dev/null | awk '{print $$1 - 1}')

DEBUG_CFLAGS := \
	-g3 -O0 \
	-Wpedantic -Werror -Wextra -Wno-unused-parameter \
	-Wshadow -Wconversion -Wsign-conversion \
	-Wimplicit-fallthrough -Wdouble-promotion \
	-Wformat=2 -Wnull-dereference -Wnon-virtual-dtor \
	-fsanitize=address,undefined \
	-fsanitize-address-use-after-scope \
	-fno-omit-frame-pointer

DEBUG_LDFLAGS := \
	-g3 \
	-fno-optimize-sibling-calls \
	-fsanitize=address,undefined \
	-fsanitize-address-use-after-scope \
	-fno-omit-frame-pointer

.PHONY: clean conan build run help

clean: ## Cleans build artifacts
	rm -rf build ~/.conan2/p

conan: ## Installs dependencies
	conan install . \
		--output-folder=build \
		--build=missing \
		--profile=$(PROFILE) \
		--settings build_type=$(BUILDTYPE)

build: ## Builds the project
	cmake -S . -B build \
		-DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake \
		-DCMAKE_BUILD_TYPE=$(BUILDTYPE) \
		-DCMAKE_C_FLAGS_DEBUG="$(DEBUG_CFLAGS)" \
		-DCMAKE_CXX_FLAGS_DEBUG="$(DEBUG_CFLAGS)" \
		-DCMAKE_EXE_LINKER_FLAGS_DEBUG="$(DEBUG_LDFLAGS)" \
		$(EXTRA_FLAGS)
	cmake --build build \
		--parallel $(NCPUS) \
		--config $(BUILDTYPE) \
		--verbose

run: build ## Builds and runs the project
	CARTRIDGE=$(CARTRIDGE) lldb -o run -- ./build/carimbo

help:
	@awk 'BEGIN {FS = ":.*?## "} /^[a-zA-Z_-]+:.*?## / {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}' $(MAKEFILE_LIST)
