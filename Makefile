VENV := .venv
PY := $(VENV)/bin/python
WEST := $(VENV)/bin/west
export ZEPHYR_TOOLCHAIN_VARIANT := zephyr

BOARD := blackpill_f411ce

.PHONY: setup build menuconfig debug flash monitor

setup:
	rm -rf $(VENV)
	python3 -m venv $(VENV)
	$(PY) -m pip install --upgrade pip
	$(PY) -m pip install west
	$(WEST) init -l .
	$(WEST) update
	$(WEST) zephyr-export
	$(WEST) packages pip --install
	$(WEST) sdk install

build:
	$(WEST) build -p always -b $(BOARD) apps/gantry -d build

menuconfig:
	$(WEST) build -t menuconfig

debug:
	$(WEST) debug --board $(BOARD) --runner openocd -- --cmd-pre-init "reset_config none"

flash:
	 $(WEST) flash --board $(BOARD)
	 #$(WEST) flash --board $(BOARD) --runner openocd -- --cmd-pre-init "reset_config none" 

monitor:
	picocom -b 115200 /dev/ttyACM0

