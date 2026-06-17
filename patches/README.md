# ZMK patches — USB OTG HS + Studio RPC over USB-next

These patches add what the `keyboard_h723zg` board (STM32H723, USB OTG HS) needs and
that upstream ZMK `main` does not provide: the USB device-next stack wiring for OTG HS,
the `studio-rpc-usb-uart-next` snippet, and related USB/HID changes. Without them the
board fails to build (`CONFIG_ZMK_USB_STACK_NEXT` undefined, no OTG HS support).

## Base

Apply on top of upstream `zmkfirmware/zmk` `main` at:

    b66a8cc8c1dc2992bdc17ee2e839e08b599190b1

This is the exact commit the patches were generated against. The result is equivalent
to the local `studio-rpc-usb-next` branch tip `1ba09822`, verified by reapplying the
series to `main` and diffing (empty).

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
