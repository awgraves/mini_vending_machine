VENV := .venv
PY := $(VENV)/bin/python
WEST := $(VENV)/bin/west
export ZEPHYR_TOOLCHAIN_VARIANT := zephyr

BOARD := blackpill_f411ce

.PHONY: setup build menuconfig debug flash

setup:
	rm -rf $(VENV)
	python3 -m venv $(VENV)
	$(PY) -m pip install --upgrade pip
	$(PY) -m pip install west==1.5.0
	$(WEST) init -l .
	$(WEST) update
	$(WEST) zephyr-export
	$(WEST) packages pip --install
	$(WEST) sdk install --version $$(cat zephyr/SDK_VERSION)

build:
	$(WEST) build -p always -b $(BOARD) apps/blinky -d build

menuconfig:
	$(WEST) build -t menuconfig

debug:
	$(WEST) debug --board $(BOARD) --runner openocd

flash:
	$(WEST) flash --board $(BOARD)
	# $(WEST) flash --board $(BOARD) --runner openocd
