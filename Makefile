SHELL := /usr/bin/env bash

.SHELLFLAGS := -euo pipefail -c
.DEFAULT_GOAL := help

PROJECT := carimbo
BUILD   := build
ROM 		:= cartridge.rom
BINARY    := $(BUILD)/$(PROJECT)
TOOLCHAIN := $(BUILD)/conan_toolchain.cmake
PACKAGES  := "$$(conan config home)/p"

PROFILE ?= $(or $(profile),.github/conan/macos)
JOBS    ?= $(shell sysctl -n hw.ncpu 2>/dev/null | awk '{print $$1 - 1}')

MODE      := Debug

SANITIZERS := \
	-fsanitize=address,undefined,nullability,implicit-conversion,float-divide-by-zero,local-bounds \
	-fsanitize-address-use-after-scope \
	-fno-omit-frame-pointer

CFLAGS := \
	-g3 -O0 \
	-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3 \
	$(SANITIZERS) \
	-fstack-protector-strong \
	-ftrivial-auto-var-init=pattern

LDFLAGS := \
	-g3 \
	-fno-optimize-sibling-calls \
	$(SANITIZERS)

LLDB := lldb --batch \
	-o version \
	-o run \
	-o 'script lldb.debugger.HandleCommand("quit " + str(lldb.process.GetExitStatus()))' \
	-k 'thread backtrace all' \
	-k 'frame variable' \
	-k 'register read' \
	-k 'image list' \
	-k 'quit 1' --

.PHONY: build clean conan help run

clean: ## Cleans build artifacts
	rm -rf $(BUILD) $(PACKAGES)

conan: clean ## Installs dependencies
	conan export recipes/luajit --version=2.1-20260908
	conan install . \
		--output-folder=$(BUILD) \
		--build="*" \
		--profile:all=$(PROFILE) \
		--settings:all build_type=$(MODE) \
		--conf:all="tools.build:jobs=$(JOBS)"

build: ## Builds the project
	cmake -S . -B $(BUILD) \
		-DCMAKE_TOOLCHAIN_FILE=$(TOOLCHAIN) \
		-DCMAKE_BUILD_TYPE=$(MODE) \
		-DCMAKE_C_FLAGS="$(CFLAGS)" \
		-DCMAKE_CXX_FLAGS="$(CFLAGS)" \
		-DCMAKE_EXE_LINKER_FLAGS="$(LDFLAGS)" \
		$(EXTRA_FLAGS)
	cmake --build $(BUILD) \
		--parallel $(JOBS) \
		--config $(MODE) \
		--verbose

run: build ## Builds and runs the project
	rm -f $(ROM)
	uv run assets/tools/run.py
	uv run tools/pack.py
	WINDOWED=1 ASAN_OPTIONS="handle_abort=1$${ASAN_OPTIONS:+:$${ASAN_OPTIONS}}" UBSAN_OPTIONS="print_stacktrace=1$${UBSAN_OPTIONS:+:$${UBSAN_OPTIONS}}" $(LLDB) $(BINARY)

help: ## Shows available commands
	@awk 'BEGIN {FS = ":.*?## "} /^[a-zA-Z_-]+:.*?## / {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}' $(MAKEFILE_LIST)
