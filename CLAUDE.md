# CLAUDE.md

See README.md for hardware, wiring and the build/flash/serve commands. This file
covers only what bit us before and is not obvious from reading the code.

## Board

Ported from a Seeed XIAO ESP32-S3 to a **LilyGO T-Energy S3**. PlatformIO has no
board definition for it — `esp32-s3-devkitc-1` is the right stand-in.

- `-DARDUINO_USB_CDC_ON_BOOT=1` is **required** in `build_flags`. The XIAO board
  definition set it implicitly; devkitc-1 does not, and without it `Serial`
  produces nothing over the native USB-C and the board looks dead.
- Battery sense is **IO3** on this board, not IO4 (IO4 is the battery pin on the
  T-Display-S3, which is a different board — most search results are about that
  one). IO4 is free here and is what the LED strip uses.
- The battery ADC reads the charger rail, not the cell, while USB-C is plugged
  in. A "wrong" voltage on USB is expected, not a bug.

## Battery cutoff

`checkBattery()` in `main.cpp` deep-sleeps below `BATTERY_CUTOFF_MV`. This is
load-bearing, not a nicety: the cells in use are unprotected flat tops with no
cutoff of their own, and running one flat damages it and makes recharging it
hazardous. Do not weaken or remove it.

The `mv > BATTERY_FLOOR_MV` guard exists so a floating or misread ADC pin can't
put the board into an immediate sleep loop on boot, which is indistinguishable
from a brick.

Percent comes from a curve in `battery.h`, not a linear map — li-ion sits
between 3.7V and 3.9V for most of its usable charge, so a straight line reads
"half full" on a nearly dead pack.

## Audio is stubbed

`initAudio()` is a no-op and `sampleAudio()` writes zeros. The FFT and band
math below it are real and tuned — keep them. `arduinoFFT` stays in `lib_deps`
for that reason even though nothing currently feeds it.

`SAMPLE_RATE` and `FFT_SAMPLES` must not be changed casually: the bin ranges in
`analyzeFrequencies()` are hand-calibrated against those exact values.

## BPM-synced motion

All tempo-driven movement goes through `advancePhase()` in `effects.cpp`, which
accumulates a float against real elapsed milliseconds. Two rules came out of
things that looked wrong on the actual horns:

- **Never step a value once per beat.** Rainbow used to do `hue += 8` on the
  beat and hold still between; that reads as stutter, not tempo.
- **Never put tempo into brightness.** Chase used to scale every pixel by a
  BPM-synced cosine; that reads as flicker, not beat.

Tempo belongs in motion, applied continuously. New BPM-synced effects should
call `advancePhase()` rather than growing a third timing scheme.

## Colour gamma

`getColor1/2/3()` in `ble_control.cpp` apply `applyGamma_video(…, COLOR_GAMMA)`.
Colours arrive as screen hex (sRGB, gamma-encoded) but WS2812s are near-linear
in PWM, so writing sRGB straight through drives the dark channel of a colour far
too hot and everything reads pastel. Correction lifts the stock purple from 51%
to 79% saturation.

- The raw value stays in the characteristic, so the app reads back what it
  wrote. Don't "fix" this by storing the corrected value — the picker would
  drift darker on every reconnect.
- Rainbow is deliberately **not** corrected. `fill_rainbow` goes through
  `hsv2rgb_rainbow`, which is already tuned for LEDs.
- `golden.cpp` stubs the colour getters, so the golden test does **not** cover
  this path. Passing tests say nothing about gamma.

## Two-colour chase

Sending **black as colour 3** means "two-colour chase" — `effectChase` drops to
two segments. This rides on the existing colour characteristic rather than
adding to the BLE protocol, at two costs worth knowing:

- An intentionally black segment (a moving gap) is unreachable.
- It also blanks the high zone of the spectrum effect. Moot while there's no
  mic, but it will matter once one is attached.

Add a colour-count characteristic if either becomes a problem.

## Tests

`./test/run.sh` runs natively, no board attached:

- `battery_test.cpp` — asserts on the SoC curve.
- `golden.cpp` — renders effects and byte-compares against `golden.txt`.

The golden test fails on **any** visual change to `effects.cpp`, including
intended ones. Re-baseline with `./test/run.sh --bless`, and only when the
change was deliberate.

First run compiles FastLED natively and takes about a minute; after that it's
cached in `.pio/native-obj`. The script globs `.pio/libdeps/*/` for FastLED and
takes the first match, so a stale env directory left over from an old board can
silently win. If the tests behave oddly after a board change, delete the stale
`.pio/libdeps/<old-env>` and `.pio/build/<old-env>`.

## Web controller

`web/index.html` is one self-contained file, hand-edited, no build step. Web
Bluetooth requires a secure context, so `file://` silently fails — always serve
it. Battery uses the standard `battery_service` / `battery_level` UUIDs, which
must be listed in `optionalServices` on `requestDevice` or the read throws.

## Conventions

Deliberate shortcuts are marked with `ponytail:` comments naming the ceiling and
the upgrade path. `/ponytail-debt` harvests them.
