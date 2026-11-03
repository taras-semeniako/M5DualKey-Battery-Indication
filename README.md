# Chain DualKey

## Overview

### SKU:C147

Chain DualKey is a programmable dual-key input development board equipped with the ESP32-S3FN8 main control chip. The front integrates 2 hot-swappable blue switch mechanical keyboard keys and 2 programmable RGB LEDs, providing excellent interactive feedback. It has a built-in 350mAh lithium battery, combining with a low-power design for good battery life. The product comes with pre-installed Chain macro keyboard firmware, supports USB / BLE connections, and can emulate HID input devices. After the device is powered on, you can connect to the device's AP hotspot and configure the HID function mapping for the local device or expansion nodes via the built-in web page to achieve various control functions. This development board adopts the M5Stack Chain series expandable design, featuring two HY2.0-4P expansion ports that support lateral expansion and connection to other sensor devices. With the USB-OTG peripheral function built into ESP32-S3, it is suitable for smart home, keyboard peripherals, macro keyboards, and other scenarios.

## build environment
esp-idf version: 5.5.1

## Battery indicator

The embedded Web page includes a **Battery Indicator** panel. Enable it, select
exactly one key (left or right), edit the battery rules, and select **Save indicator
settings**. The feature is disabled by default.

Each rule specifies an inclusive minimum/maximum battery percentage, an RGB color,
and a repeat interval in seconds. You can add or remove rules (up to eight).
Percentages must be integers from 0 to 100; intervals must be integers from 1 to
86400 seconds. Overlapping ranges, including shared endpoints, are rejected.
Uncovered percentages deliberately produce no battery indication.

The initial example rules are:

| Battery percentage | Color | Repeat interval |
| --- | --- | --- |
| 76–100% | Green | 30 seconds |
| 30–75% | Yellow | 60 seconds |
| 0–19% | Red | 10 seconds |

Each flash lasts a fixed **300 ms**. Ordinary key colors and RGB effects continue
between flashes; the other key is unaffected. Reset/long-press status feedback
takes priority. Choose a color that contrasts with normal key lighting so the
flash is visible. Indication pauses while USB is connected or RGB lighting is
switched off. After enabling, saving, or resuming, the first flash waits one full
interval. Range changes use the current rule's interval measured from the last
flash, so threshold fluctuations do not generate additional flashes. Battery
percentage remains the existing voltage-based estimate, not a precision fuel gauge.

Settings are saved separately from key mappings in a versioned, CRC-protected NVS
record and restored after reboot. Scheduling runs on the device and continues
after the Web page closes. The page confirms successful saves and displays
validation or persistence errors without discarding unsaved edits.

### Regression checks

With Node.js and MSVC available in a developer PowerShell, and managed components
restored by `idf.py reconfigure`, run:

```powershell
.\tests\run_host_tests.ps1
idf.py build
```

The host script exercises the C rule scheduler, the real BSP LED overlay with a
fake hardware transport, and Web validation/save/reconnection behavior. It accepts
`-Compiler`, `-Linker`, and `-CompilerInclude` for an existing non-PATH MSVC setup.
Physical LED timing, USB pause/resume and reboot persistence should also be checked
on a DualKey after flashing a build made with ESP-IDF 5.5.1.

## Related Link

- [Document & Datasheet](https://docs.m5stack.com/en/chain/Chain_DualKey)

## Required Libraries:

- [M5Chain](https://github.com/m5stack/M5Chain)

## License

- [Product Name- MIT](LICENSE)
