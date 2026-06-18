# ZMK patches — USB OTG HS + Studio RPC over USB-next

These patches add what the Vadox V1 board (`vadox_v1`, STM32H723, USB OTG HS) needs and
that upstream ZMK `main` does not provide: the USB device-next stack wiring for OTG HS,
the `studio-rpc-usb-uart-next` snippet, and related USB/HID changes. Without them the
board fails to build (`CONFIG_ZMK_USB_STACK_NEXT` undefined, no OTG HS support).

## Base

Apply on top of upstream `zmkfirmware/zmk` `main` at:

    b66a8cc8c1dc2992bdc17ee2e839e08b599190b1

This is the exact commit the patches were generated against. Patches `0001`–`0005`
reproduce the branch tip `1ba09822`; `0006` extends it to `3ac91dce`; `0007` extends it
to `9e16ba17` (current tip).

## Apply

    cd ~/zmk
    git checkout -b studio-rpc-usb-next b66a8cc8
    git am /path/to/patches/000*.patch

The series applies cleanly and authorship/messages are preserved. The original branch
also contained a same-branch merge commit that introduced no unique content, so it is
intentionally omitted.

## Files

- `0001-initial-attempt-to-enable-usbotg.patch`
- `0002-studio-rpc.patch`
- `0003-another-attempt.patch`
- `0004-another-studio-attempt.patch`
- `0005-configs-changed.patch`
- `0006-usb-include-logging-log.h-before-LOG_MODULE_DECLARE.patch` — lets the **legacy**
  USB stack compile (it was relying on `log.h` arriving via the next-stack header). Needed
  for the RP2040 keypad, which falls back to the legacy stack because the USB-next
  `udc_rpi_pico` controller does not enumerate at runtime on this revision.
- `0007-keymap-runtime-sensor-encoder-binding-get-set-persis.patch` — adds a runtime
  sensor-binding get/set API to `keymap.c` (mirroring the matrix runtime keymap setters)
  so the configurator can edit encoder sensor-bindings live, with settings persistence
  (`keymap/s/<layer>/<sensor>`). No-op on boards without sensors (e.g. Vadox V1).
