JORNADA_TOOLCHAIN ?= $(error Set JORNADA_TOOLCHAIN to the jornada-72x-tools directory)
BUILD ?= build/ce
VERSION ?= 0.1.0
INSTALL_PATH ?= /Storage Card/アプリ/NetSurf

.PHONY: all sources package install clean

all: sources
	cmake -S . -B $(BUILD) -G Ninja -DCMAKE_TOOLCHAIN_FILE=$(JORNADA_TOOLCHAIN)/cmake/jornada-ce.cmake -DJORNADA_TOOLCHAIN=$(JORNADA_TOOLCHAIN) $(CMAKE_ARGS)
	cmake --build $(BUILD)

sources:
	node tools/fetch-sources.mjs

package: all
	python3 tools/package.py --build $(BUILD) --sources sources --folder build/package --zip build/netsurf-ce-$(VERSION).zip
	@echo build/netsurf-ce-$(VERSION).zip

install: package
	node tools/install.mjs "$(JORNADA_TOOLCHAIN)/tools/jornada-debug" "$(or $(BRAIN_DEVICE),$(error Set BRAIN_DEVICE to the debug agent address))" "build/package/アプリ/NetSurf" "$(INSTALL_PATH)"

clean:
	rm -rf build
