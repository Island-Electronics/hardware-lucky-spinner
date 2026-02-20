# Pinout

This document reflects the current hardware wiring used by the firmware.

## Digital Pins

| Pin | Function | Notes |
| --- | --- | --- |
| D2 | Button `DOWN` | Uses `INPUT_PULLUP` (pressed = LOW) |
| D4 | Button `RIGHT` | Uses `INPUT_PULLUP` (pressed = LOW) |
| D6 | Button `UP` | Uses `INPUT_PULLUP` (pressed = LOW) |
| D7 | Button `LEFT` | Uses `INPUT_PULLUP` (pressed = LOW) |
| D8 | NeoPixel data | Strip with 3 RGB LEDs (`NEO_GRB + NEO_KHZ800`) |
| D9 | Buzzer | Driven with `tone()` |

## I2C

- OLED: SSD1306 128x32
- Interface: standard I2C (`SDA`, `SCL`)
- I2C address: `0x3C`
- Reset pin: not connected (`OLED_RESET = -1`)
