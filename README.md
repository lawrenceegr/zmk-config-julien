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
cd ~/zmk/app
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

## RGB Hardware

Per-key RGB is driven by two **ISSI IS31FL3733-QF** LED matrix controllers (88 LEDs total).

| IC | Label | I2C Bus | Zephyr I2C | Address | Sync Role | LEDs |
|----|-------|---------|------------|---------|-----------|------|
| U3 | `led_driver1` | SDA1 / SCL1 (PF15 / PF14) | `i2c4` | 0x50 | Master | 64 (SW1–SW12 × CS1–CS16) |
| U4 | `led_driver2` | SDA2 / SCL2 (PF0 / PF1)  | `i2c2` | 0x50 | Slave  | 24 (SW1–SW6  × CS1–CS14) |

Both drivers share a **LEDS_SHUTDOWN** net connected to their SDB pins (active-high enable) on **PD11**. `sdb-gpios = <&gpiod 11 GPIO_ACTIVE_HIGH>` is set on both DTS nodes.

### LED addressing (IS31FL3733)

Each driver has a 12-row × 16-column matrix. LED index within a driver:

```
index = (SW_row - 1) * 16 + (CS_col - 1)
```

Per-key RGB mapping (which matrix position corresponds to which physical key) is TBD and will be configured once the full LED layout is confirmed.

### ZMK RGB integration status

ZMK's `zmk,rgb-underglow` expects a `led_strip`-API device. IS31FL3733 uses Zephyr's `led` API. A thin bridge ZMK module is needed to expose the two IS31FL3733 devices as a combined LED strip. This is planned but not yet implemented — the DTS and Kconfig are in place as the hardware foundation.

## Keymap

### Default Layer

Standard QWERTY layout with full-size 104-key arrangement.

### RGB Layer (hold second RCTRL)

#### F Row — Media Keys

| Key | Action |
|-----|--------|
| F1  | Screen brightness down |
| F2  | Screen brightness up |
| F5  | Previous track |
| F6  | Play / Pause |
| F7  | Next track |
| F8  | Stop |
| F9  | Mute |
| F10 | Volume down |
| F11 | Volume up |

#### Letter Keys — RGB Control

| Key | Action |
|-----|--------|
| Q   | Toggle RGB |
| W   | Brightness down |
| E   | Brightness up |
| R   | Effect previous |
| T   | Effect next |
| A   | Saturation down |
| S   | Saturation up |
| D   | Hue down |
| F   | Hue up |

Number row is reserved for lighting mode selection (to be mapped).

## Configuration Files

- `config/boards/arm/keyboard_h723zg/` - Board definition
  - `keyboard_h723zg.dts` - Main devicetree (GPIO pins, matrix, LEDs, sensors, etc.)
  - `keyboard_h723zg_defconfig` - Kconfig options (BLE, RGB, etc.)
  - `keyboard_h723zg-pinctrl.dtsi` - Pin control configuration
  - `keyboard_h723zg.keymap` - Key mappings
