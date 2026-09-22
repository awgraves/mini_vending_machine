{ pkgs, lib, config, inputs, ... }:
let
  zephyr = inputs.zephyr-nix.packages.${pkgs.system};
  zephyr-sdk = zephyr.sdk.override {
    targets = ["arm-zephyr-eabi"];
  };
in
{
  # https://devenv.sh/basics/
  
  env.GREET = "Mini Vending Machine DevEnv";
  env.ZEPHYR_TOOLCHAIN_VARIANT = "zephyr";
  env.ZEPHYR_SDK_INSTALL_DIR = "${zephyr-sdk}";

  packages = [
    pkgs.cmake
    pkgs.ninja
    pkgs.dtc
    pkgs.dfu-util
    pkgs.picocom

    zephyr.pythonEnv
    zephyr.hosttools-nix
    zephyr-sdk
  ];

  languages = {
    c = {
      enable = true;
      lsp = {
        enable = true;
        package = pkgs.clang-tools;
      };
    };
  };


  # https://devenv.sh/basics/
  enterShell = ''
    if [ ! -d .venv ]; then
      echo "No .venv found, running first-time setup (this may take a while)..."
      make setup
    fi
  '';

}
