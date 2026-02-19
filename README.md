# ZMK Config - Keyboard

ZMK firmware configuration for Custom STM32H723ZG-based Keyboard.

## Prerequisites

- [Zephyr SDK 0.17.2+](https://github.com/zephyrproject-rtos/sdk-ng/releases)
- [ZMK](https://zmk.dev/docs/development/setup) development environment
- `protobuf-compiler` (for ZMK Studio support)
- OpenOCD (for ST-Link flashing)

```bash
sudo apt-get install protobuf-compiler openocd
```

## Building

1. Activate the ZMK virtual environment:

```bash
cd ~/zmk
source .venv/bin/activate
```

2. Build the firmware:

```bash
west build -b keyboard_h723zg --   -DZMK_CONFIG="/home/marcus/keyboards-firmware/zmk-config-julien/config"   -DZMK_EXTRA_MODULES="/home/marcus/keyboards-firmware/zmk-config-julien"
```

3. Output files are located in `~/zmk/app/build/zephyr/`:
   - `zmk.hex` - Intel HEX format
   - `zmk.uf2` - UF2 format (for bootloader flashing)

## Flashing

### Using ST-Link (OpenOCD)

```bash
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
  -c "program build/zephyr/zmk.hex verify reset exit"
```

### Using J-Link

```bash
west flash --runner jlink
```

### Using nrfjprog

```bash
west flash --runner nrfjprog
```

## Clean Build

To perform a clean build, remove the build directory first:

```bash
rm -rf ~/zmk/app/build
```

Then run the build command again.

## Configuration Files

- `config/boards/arm/keyboard_h723zg/` - Board definition
  - `keyboard_h723zg.dts` - Main devicetree (GPIO pins, matrix, LEDs, sensors, etc.)
  - `keyboard_h723zg_defconfig` - Kconfig options (BLE, RGB, etc.)
  - `keyboard_h723zg-pinctrl.dtsi` - Pin control configuration
  - `keyboard_h723zg.keymap` - Key mappings
