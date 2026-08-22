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

## Audio comes from the phone

There is no DSP on the board and there should not be one. The old `audio.cpp`
ran a 2048-point **double-precision** FFT inside the 60fps render loop; the
S3's FPU is single-precision only, so doubles are software-emulated, and just
collecting 2048 samples at 16kHz costs 128ms. That design would have dropped
the sound-reactive effects to single-digit fps the day a mic was attached. It
was deleted, along with `arduinoFFT`, and took 32KB of RAM with it.

The phone analyses the room and writes four bytes — `kick, bass, mid, high` —
to `AUDIO_CHAR_UUID` at roughly 25Hz. `ble_control.cpp` is the whole receiving
end.

- **Stale audio reads as silence**, not as the last frame. Without
  `AUDIO_STALE_MS` a phone that locks, backgrounds or wanders out of range
  leaves a sound-reactive effect stuck lit, which reads as a crash. Don't
  "optimise" the check away because the values look constant in a test.
- **`getKick()` is an onset, not a level.** See the next section.
- `updateAudioState()` must be called once per frame — it advances the slow
  tilt EMA. Calling it from a getter instead would advance it once per *use*.
- The web app must write with `writeValueWithoutResponse`, which is why
  `addChar()` grants `WRITE_NR`. A with-response write per frame queues up and
  chokes the link.

## Level is not impact

The rule that makes the sound-reactive effects work on bass music, and the one
most likely to get undone by someone "simplifying" it.

A reece, a wobble, a held sub — these keep `getBassEnergy()` pinned high for an
entire drop while the kick is still a series of distinct hits. Drive brightness
from the level and a dubstep track reads as *bright the whole time*, with no
beat in it at all. `getKick()` is the rise above a ~300ms baseline, computed on
the phone at audio rate because at 25Hz a transient is one or two samples and
the derivative is mostly noise.

Generally: **transients belong in brightness, sustained energy belongs in
colour and texture.** The "tempo belongs in motion, never in brightness" rule
above is the same rule — a slowly-varying quantity in brightness reads as
flicker, whatever its source.

Spectral tilt (`getTilt()`, -1 all sub to +1 all air) is the sustained half. It
is smoothed over seconds deliberately, so it tracks the track rather than the
moment, and it moves along the user's own colour ramp via `tiltColor()` rather
than inventing hues — generating a hue would fight the colour picker.

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

## Segmented and smooth are both modes

Spectrum and Flow read the same three bands and differ only in whether the
boundaries are hard. Spectrum steps between three zones, which makes each band
legible on its own; Flow interpolates position continuously against frequency,
which removes the seams — across one frame the largest jump between neighbouring
pixels drops from 360 to 18.

Neither is the "fixed" version of the other. Don't collapse them.

## Colour count

`getColorCount()` returns 2 or 3 and says how many colours an effect should
use. It replaced an older trick where **black as colour 3** was the signal,
which cost two things: an intentionally black segment (a moving gap) was
unreachable, and it silently blanked the high zone of the spectrum effect.

Don't reintroduce sentinel colours. Black is an ordinary colour now.

`golden.cpp`'s two-colour block deliberately leaves colour 3 at a real value,
so if `effectChase` ever starts reading `colors[2]` with a count of 2 the test
catches it.

## Tests

`./test/run.sh` runs natively, no board attached:

- `battery_test.cpp` — asserts on the SoC curve.
- `golden.cpp` — renders effects and byte-compares against `golden.txt`.
- `web.js` — checks `web/index.html`, which nothing else checks. Skipped with a
  notice if `node` isn't installed.

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
Bluetooth **and `getUserMedia`** both require a secure context, so `file://`
silently fails — always serve it. `localhost` counts as secure, so desktop
Chrome over `python3 -m http.server` exercises the whole loop without a deploy.

The mic is requested with `autoGainControl`, `echoCancellation` and
`noiseSuppression` all **off**. Those defaults are tuned for speech; AGC in
particular pumps sub against mids and flattens the spectral tilt to nothing.

The BLE write for audio is guarded by an in-flight flag, which doubles as the
rate limiter — Web Bluetooth queues writes, so driving them from `rAF`
unguarded builds an unbounded backlog that surfaces minutes later as lag.

The band edges and `maxDecibels` are **measured, not derived**. Probed with
`web/mictest.html` against real tracks through a phone mic in a room:

    40Hz  -69dB  31dB of swing      2.5k  -95dB   4dB
    630Hz -85dB  15dB               5k    -98dB   2dB   <- floored
    1.25k -90dB  10dB              10k    -97dB   3dB   <- floored

Everything above ~2.5kHz sits within a few dB of the analyser's floor — a phone
mic, a room, and bass-heavy material leave nothing rhythmic up there. A textbook
2–8kHz "high" band averages pure noise and reads as permanent silence, which
also pins spectral tilt at -1 and kills the colour drift entirely. The usable
brightness information is 630Hz–2.5kHz, roughly two octaves below where you
would put it from theory.

Narrowing `maxDecibels` is a **gain control**, and the direction is
counter-intuitive: a *wider* dB window means *fewer bytes per dB*, and byte span
is what the band tracker feeds on. Widening it to "capture more range" makes
quiet bands worse. Re-run the probe if the mic or the venue changes.

Tempo detection searches 90–179 BPM. A range narrower than one octave *cannot*
produce an octave error, which is autocorrelation's classic failure — 140 BPM
confidently reported as 70. Don't widen it without replacing the mechanism.

There is no linter or build step, so `test/web.js` (run by `./test/run.sh`) is
this file's only check. It evaluates the `<script>` block's top level against
DOM stubs — catching undeclared identifiers and `getElementById` typos — and
asserts the GATT gate above actually serialises. Battery uses the standard `battery_service` / `battery_level` UUIDs, which
must be listed in `optionalServices` on `requestDevice` or the read throws.

Deployed by GitHub Pages from `main`, so a push is the deploy. Bluefy
hard-requires HTTPS — there is no plain-HTTP LAN fallback for iOS, so changes
have to be pushed to be testable on the phone. See the README for the URL and
why the project-level custom domain is load-bearing.

`sw.js` is network-first on purpose. Cache-first would strand the page on an old
build, which is the failure this project is most likely to hit: the device gets
used far from any computer, so a wedged cache can't be cleared by hand.

## Conventions

Deliberate shortcuts are marked with `ponytail:` comments naming the ceiling and
the upgrade path. `/ponytail-debt` harvests them.
