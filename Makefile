.PHONY: help test build build-all build-supermini build-n16r8 \
        flash-supermini flash-n16r8 monitor erase-supermini erase-n16r8 \
        devices size package lint-docs check clean

PIO ?= pio
PYTHON ?= python3
PORT ?=
BAUD ?= 115200
DIST_DIR ?= dist

PORT_FLAG = $(if $(PORT),--upload-port $(PORT),)
MON_PORT_FLAG = $(if $(PORT),--port $(PORT),)

help:
	@echo "RubberOrca (ESP32-S3) Build and Operations"
	@echo ""
	@echo "Build and Test:"
	@echo "  make test              Run host-side Unity protocol/command tests (native)"
	@echo "  make build             Build firmware for both ESP32-S3 targets"
	@echo "  make build-supermini   Build ESP32-S3 Super Mini firmware"
	@echo "  make build-n16r8       Build ESP32-S3 N16R8 DevKitC firmware"
	@echo "  make size              Show Flash and RAM memory usage for both targets"
	@echo "  make package           Build and stage factory .bin files + SHA256SUMS in $(DIST_DIR)/"
	@echo ""
	@echo "Hardware Operations (pass PORT=/dev/... to override auto-detect):"
	@echo "  make devices           List connected USB serial devices"
	@echo "  make flash-supermini   Flash ESP32-S3 Super Mini"
	@echo "  make flash-n16r8       Flash ESP32-S3 N16R8 DevKitC"
	@echo "  make monitor           Open serial monitor ($(BAUD) baud)"
	@echo "  make erase-supermini   Erase flash and NVS on Super Mini"
	@echo "  make erase-n16r8       Erase flash and NVS on N16R8"
	@echo ""
	@echo "Quality and Cleanup:"
	@echo "  make lint-docs         Verify governance files, anti-slop rules, and markdown links"
	@echo "  make check             Run full local CI suite (test + build + lint-docs)"
	@echo "  make clean             Remove PlatformIO build cache and $(DIST_DIR)/"

test:
	$(PIO) test -e native

build-supermini:
	$(PIO) run -e esp32s3_supermini

build-n16r8:
	$(PIO) run -e esp32s3_n16r8

build: build-supermini build-n16r8
build-all: build

size:
	$(PIO) run -e esp32s3_supermini -t size
	$(PIO) run -e esp32s3_n16r8 -t size

package: build
	@mkdir -p $(DIST_DIR)
	cp .pio/build/esp32s3_supermini/firmware.factory.bin $(DIST_DIR)/rubberorca-esp32s3-supermini-factory.bin
	cp .pio/build/esp32s3_n16r8/firmware.factory.bin $(DIST_DIR)/rubberorca-esp32s3-n16r8-factory.bin
	@(cd $(DIST_DIR) && shasum -a 256 rubberorca-*.bin > SHA256SUMS.txt)
	@echo "Staged release binaries in $(DIST_DIR)/:"
	@ls -lh $(DIST_DIR)/

devices:
	$(PIO) device list

flash-supermini:
	$(PIO) run -e esp32s3_supermini -t upload $(PORT_FLAG)

flash-n16r8:
	$(PIO) run -e esp32s3_n16r8 -t upload $(PORT_FLAG)

monitor:
	$(PIO) device monitor -b $(BAUD) $(MON_PORT_FLAG)

erase-supermini:
	$(PIO) run -e esp32s3_supermini -t erase $(PORT_FLAG)

erase-n16r8:
	$(PIO) run -e esp32s3_n16r8 -t erase $(PORT_FLAG)

lint-docs:
	@for f in README.md LICENSE CONTRIBUTING.md SECURITY.md ROADMAP.md; do \
		test -f "$$f" || (echo "Missing $$f" && exit 1); \
	done
	@$(PYTHON) -c '\
import re, glob, os, sys; \
forbidden = re.compile(r"seamlessly|blazingly fast|cutting-edge|revolutionary|comprehensive|effortlessly|ultimate|battle-tested", re.I); \
errors = []; \
[errors.append(f"em-dash in {f}") for f in sorted(glob.glob("**/*.md", recursive=True)) if not f.startswith(".pio/") and "—" in open(f, encoding="utf-8").read()]; \
[errors.append(f"slop in {f}") for f in sorted(glob.glob("**/*.md", recursive=True)) if not f.startswith(".pio/") and forbidden.search(open(f, encoding="utf-8").read())]; \
[errors.append(f"broken link {f} -> {l}") for f in sorted(glob.glob("**/*.md", recursive=True)) if not f.startswith(".pio/") for l in re.findall(r"\[.*?\]\((?!https?://|mailto:)(.*?)(?:#.*)?\)", re.sub(r"`[^`]*`", "", re.sub(r"```[\s\S]*?```", "", open(f, encoding="utf-8").read()))) if l.strip().split("?")[0] and not l.strip().startswith("#") and not os.path.exists(os.path.normpath(os.path.join(os.path.dirname(f), l.strip().split("?")[0])))]; \
sys.exit("\n".join(errors)) if errors else print("Docs and links OK.")'

check: test build lint-docs

clean:
	$(PIO) run -t clean
	rm -rf $(DIST_DIR)
