VENV := .venv
PY   := $(VENV)/bin/python
WEST := $(VENV)/bin/west
export ZEPHYR_TOOLCHAIN_VARIANT := zephyr

APPS  := gantry dispenser controller

# Per-app boards
BOARD_gantry := blackpill_f411ce
BOARD_dispenser := blackpill_f411ce
BOARD_controller := doit_esp32_devkit_v1/esp32/procpu

PORT := /dev/ttyACM0
BAUD := 115200

BUILD_TARGETS     := $(addprefix build-,$(APPS))
FLASH_TARGETS     := $(addprefix flash-,$(APPS))
MENUCONFIG_TARGETS := $(addprefix menuconfig-,$(APPS))
DEBUG_TARGETS     := $(addprefix debug-,$(APPS))
REBUILD_TARGETS   := $(addprefix rebuild-,$(APPS))

.PHONY: setup build flash clean monitor \
        $(BUILD_TARGETS) $(FLASH_TARGETS) $(MENUCONFIG_TARGETS) \
        $(DEBUG_TARGETS) $(REBUILD_TARGETS)

setup:
	rm -rf $(VENV)
	python3 -m venv $(VENV)
	$(PY) -m pip install --upgrade pip
	$(PY) -m pip install west
	$(WEST) init -l .
	$(WEST) update
	$(WEST) zephyr-export
	$(WEST) packages pip --install
	$(WEST) blobs fetch hal_espressif
	# NOTE: the zephyr-sdk will be installed by the devenv rather than by west

# Static pattern rules: <targets>: <target-pattern>: <prereqs>
$(BUILD_TARGETS): build-%:
	$(WEST) build -p auto -b $(BOARD_$*) apps/$* -d build/$* $(if $(EXTRA),-- $(EXTRA))

$(REBUILD_TARGETS): rebuild-%:
	$(WEST) build -p always -b $(BOARD_$*) apps/$* -d build/$*

$(FLASH_TARGETS): flash-%:
	$(WEST) flash -d build/$*

$(MENUCONFIG_TARGETS): menuconfig-%:
	$(WEST) build -d build/$* -t menuconfig

$(DEBUG_TARGETS): debug-%:
	$(WEST) debug -d build/$* --runner openocd -- --cmd-pre-init "reset_config none"

build: $(BUILD_TARGETS)

clean:
	rm -rf build

monitor:
	picocom -b $(BAUD) $(PORT)
