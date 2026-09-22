# mini_vending_machine
A desktop-sized machine that dispenses snacks via a 2-axis motorized gantry

## Local Dev Setup
Assumes a Linux host

### Requirements
[Devenv.sh](https://devenv.sh/getting-started/) is the only direct dependency.
It is a convenient way to create deterministic, reproducible dev environments with automatic tooling setup!
Devenv uses the nix package manager and will automatically download and install isolated tooling dependencies that will not pollute your host system.
See the 'steps' section below.

#### Permissions
Assuming you are on linux:
For dfu-util (openocd flashing) & picocom (serial monitor) access, there are 2 options:
1. Use sudo for `sudo make flash` and `sudo make monitor`
2a. Add udev rule:
```
  SUBSYSTEM=="tty", ATTR{idVendor}=="0483", MODE="0666", TAG+="uaccess"
```
2b. Add 'dialout' group for your user for picocom (requires complete logout to take effect).

### Steps
1. Before cloning the repo, first create a workspace dir ie `mkdir workspace`
2. `cd` into the newly created dir and git clone this repo there.
3. Ensure nix package manager and devenv is installed, then exec `devenv shell` to enter the development shell.
All software dependencies will be automatically installed.
