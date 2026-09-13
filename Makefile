SHELL := /usr/bin/env bash

.SHELLFLAGS := -euo pipefail -c
.DEFAULT_GOAL := help

PROFILE   ?= $(or $(profile),.github/conan/macos)
NCPUS     ?= $(shell sysctl -n hw.ncpu 2>/dev/null | awk '{print $$1 - 1}')

SANITIZER_FLAGS := \
	-fsanitize=address,undefined,nullability,implicit-conversion,float-divide-by-zero,local-bounds \
	-fsanitize-address-use-after-scope \
	-fno-omit-frame-pointer

DEBUG_CFLAGS := \
	-g3 -O0 \
	-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3 \
	$(SANITIZER_FLAGS) \
	-fstack-protector-strong \
	-ftrivial-auto-var-init=pattern

DEBUG_LDFLAGS := \
	-g3 \
	-fno-optimize-sibling-calls \
	$(SANITIZER_FLAGS)

.PHONY: build clean conan debug help run

clean: ## Cleans build artifacts
	rm -rf build

conan: ## Installs dependencies
	conan export recipes/luajit --version=2.1-20260908
	conan install . \
		--output-folder=build \
		--build="*" \
		--profile:all=$(PROFILE) \
		--settings:all build_type=Debug \
		--conf:all="tools.build:jobs=$(NCPUS)" \
		--conf:all="tools.build:cflags=[$(foreach flag,$(DEBUG_CFLAGS),'$(flag)',)]" \
		--conf:all="tools.build:cxxflags=[$(foreach flag,$(DEBUG_CFLAGS),'$(flag)',)]" \
		--conf:all="tools.build:exelinkflags=[$(foreach flag,$(DEBUG_LDFLAGS),'$(flag)',)]"

build: ## Builds the project
	cmake --fresh -S . -B build \
		-DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake \
		-DCMAKE_BUILD_TYPE=Debug \
		$(EXTRA_FLAGS)
	cmake --build build \
		--parallel $(NCPUS) \
		--config Debug \
		--verbose

debug: DEBUGGER = lldb --batch \
	-o version \
	-o run \
	-o 'script lldb.debugger.HandleCommand("quit " + str(lldb.process.GetExitStatus()))' \
	-k 'thread backtrace all' \
	-k 'frame variable' \
	-k 'register read' \
	-k 'image list' \
	-k 'quit 1' --
debug: run

run: build ## Builds and runs the project
	rm -f cartridge.rom
	uv run assets/tools/run.py
	uv run tools/pack.py
	mkdir -p logs
	set -o pipefail; WINDOWED=1 \
		ASAN_OPTIONS="handle_abort=1$${ASAN_OPTIONS:+:$${ASAN_OPTIONS}}" \
		UBSAN_OPTIONS="print_stacktrace=1$${UBSAN_OPTIONS:+:$${UBSAN_OPTIONS}}" \
		$(DEBUGGER) ./build/carimbo 2>&1 | tee logs/carimbo-$$(date -u +%Y%m%dT%H%M%SZ)-$$$$.log

help: ## Shows available commands
	@awk 'BEGIN {FS = ":.*?## "} /^[a-zA-Z_-]+:.*?## / {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}' $(MAKEFILE_LIST)
