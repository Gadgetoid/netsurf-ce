GRIMACE ?= $(error Set GRIMACE to the grimace directory)
BUILD ?= build/ce
VERSION ?= 0.1.0
CE_DEBUG ?= ce-debug
INSTALL_PATH ?= /Storage Card

.PHONY: all sources check-imports package install clean

all: sources
	cmake -S . -B $(BUILD) -G Ninja -DCMAKE_TOOLCHAIN_FILE=$(GRIMACE)/cmake/toolchain.cmake -DGRIMACE_ARCH=arm -DGRIMACE_CE_VERSION=6 -DGRIMACE_PLATFORM=brain $(if $(GRIMACE_LLVM_ROOT),-DGRIMACE_LLVM_ROOT=$(GRIMACE_LLVM_ROOT)) $(CMAKE_ARGS)
	cmake --build $(BUILD)

check-imports:
	python3 $(GRIMACE)/tests/check-imports.py $(BUILD) --arch arm --version 6 --device pwsh2
	python3 $(GRIMACE)/tests/check-imports.py $(BUILD) --arch arm --version 6 --device pwsh4

sources:
	node tools/fetch-sources.mjs

package: all
	python3 tools/package.py --build $(BUILD) --sources sources --folder build/package --zip build/netsurf-ce-$(VERSION).zip
	@echo build/netsurf-ce-$(VERSION).zip

install: package
	node tools/install.mjs "$(CE_DEBUG)" "$(or $(BRAIN_DEVICE),$(error Set BRAIN_DEVICE to the debug agent address))" build/package "$(INSTALL_PATH)"

clean:
	rm -rf build
