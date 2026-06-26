# Vadox — ZMK Config

ZMK firmware configuration for the Vadox custom STM32H723ZG-based unibody keyboards. Two boards
share this config and the same SoC, USB OTG HS stack, external settings flash, and per-key RGB:

| Board id | Name | Switches | Encoder | Notes |
|----------|------|----------|---------|-------|
| `vadox_v1` | Vadox V1 | ~88 (6 rows) | — | TKL, per-key RGB |
| `vadox_v2` | Vadox V2 | ~95 (7 rows) | 1 (EC11) | V1 + a 7th switch row and a rotary encoder (volume) |

Everything below applies to both boards; substitute the board id (`vadox_v1` / `vadox_v2`) and the
matching build directory where shown. The only configuration difference is that `vadox_v2` enables
the EC11 encoder driver (`CONFIG_EC11`) and adds an `alps,ec11` sensor on PD5/PD6 with a
`zmk,keymap-sensors` node; all shared Kconfig/devicetree is identical.

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

2. Build the firmware (each board into its own directory):

```bash
cd ~/zmk/app

# Vadox V1
west build -b vadox_v1 -d build_vadox_v1 -- \
  -DZMK_CONFIG="/home/marcus/vadox-workspace/keyboards/zmk-config-julien/config" \
  -DZMK_EXTRA_MODULES="/home/marcus/vadox-workspace/keyboards/zmk-config-julien;/home/marcus/vadox-workspace/modules/zmk-configurator"

# Vadox V2 (V1 + encoder + 7 switches)
west build -b vadox_v2 -d build_vadox_v2 -- \
  -DZMK_CONFIG="/home/marcus/vadox-workspace/keyboards/zmk-config-julien/config" \
  -DZMK_EXTRA_MODULES="/home/marcus/vadox-workspace/keyboards/zmk-config-julien;/home/marcus/vadox-workspace/modules/zmk-configurator"
```

The board config enables the Studio runtime-keymap stack on its own; the configurator module
(`modules/zmk-configurator`) is what Vadox Studio talks to over its dedicated CDC endpoint.

3. Output files are located in `~/zmk/app/build_vadox_v1/zephyr/` (or `build_vadox_v2/`):
   - `zmk.hex` - Intel HEX format
   - `zmk.uf2` - UF2 format (for bootloader flashing)

## Flashing

### Using ST-Link (OpenOCD)

```bash
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
  -c "program build_vadox_v1/zephyr/zmk.hex verify reset exit"
```

OpenOCD/ST-Link only reaches the STM32 internal flash (the `code` partition at `0x08000000`).
It cannot touch the external settings flash — see Troubleshooting below.

### Using J-Link

```bash
west flash --runner jlink
```

## Clean Build

To perform a clean build, remove the build directory first:

```bash
rm -rf ~/zmk/app/build_vadox_v1
```

Then run the build command again.

## Troubleshooting

### Wiping persistent settings (recover from a save-induced crash)

**Symptom seen:** after writing settings from Vadox Studio, the board crashes. Erasing the
settings partition recovers it.

**Why it can't be done with OpenOCD:** persistent settings use the **NVS** backend
(`CONFIG_SETTINGS_NVS`) stored on the **external W25Q128 SPI NOR** (on `spi5`), partition
`storage` at offset `0x0`, size `0x10000` (64 KB) — see `boards/arm/vadox_v1/vadox_v1.dts`.
OpenOCD/ST-Link can only program the STM32 internal flash, and this is a plain SPI bus (not
memory-mapped QSPI/OCTOSPI), so there is no external loader for it. The wipe must be done by the
firmware itself.

ZMK provides `CONFIG_ZMK_SETTINGS_RESET_ON_START`, which erases the settings partition once on
boot. The reset must run **after** the SPI-NOR device is initialized: `CONFIG_SPI_NOR_INIT_PRIORITY`
is `80`, but the reset's default priority is `60` (would run before the flash is ready and silently
no-op), so force it above 80.

1. Build a one-shot wipe firmware into a **separate** directory (leaves `build_vadox_v1` intact):

```bash
cd ~/zmk && source .venv/bin/activate && cd app
west build -b vadox_v1 -d build_vadox_reset -- \
  -DZMK_CONFIG="/home/marcus/vadox-workspace/keyboards/zmk-config-julien/config" \
  -DZMK_EXTRA_MODULES="/home/marcus/vadox-workspace/keyboards/zmk-config-julien;/home/marcus/vadox-workspace/modules/zmk-configurator" \
  -DCONFIG_ZMK_SETTINGS_RESET_ON_START=y \
  -DCONFIG_ZMK_SETTINGS_RESET_ON_START_INIT_PRIORITY=90
```

2. Flash it and let it boot for ~5 seconds — it erases the `storage` partition on boot (UART
   prints `Erasing settings flash partition`):

```bash
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
  -c "program build_vadox_reset/zephyr/zmk.hex verify reset exit"
```

3. Reflash the normal firmware to return to a clean settings state:

```bash
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
  -c "program build_vadox_v1/zephyr/zmk.hex verify reset exit"
```

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

Each board has its own directory under `boards/arm/` with the same file layout:

- `boards/arm/vadox_v1/`, `boards/arm/vadox_v2/` - Board definitions
  - `vadox_vX.dts` - Main devicetree (GPIO pins, matrix, LEDs, sensors, etc.)
  - `vadox_vX_defconfig` - Kconfig options (RGB, configurator, etc.)
  - `vadox_vX-layouts.dtsi` - Physical layout definitions
  - `vadox_vX.keymap` - Key mappings

The V2 devicetree adds a 7th matrix row (the 7 extra switches), an `alps,ec11` `right_encoder`
on PD5/PD6, and a `zmk,keymap-sensors` node; its keymap adds the matching `sensor-bindings`
(`&inc_dec_kp C_VOL_UP C_VOL_DN`) and a `&none`/`&trans` row for the new switches. All other
devicetree, defconfig, and `.conf` content is identical to V1.
