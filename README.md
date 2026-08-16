# Kwynumbra

Wearable LED horns. ESP32-S3 firmware driving a WS2812B strip, controlled over
BLE from a phone.

## Hardware

| | |
|---|---|
| Board | LilyGO **T-Energy S3** (ESP32-S3-WROOM, 18650 holder, USB-C) |
| LED data | **IO4** → WS2812B strip, 60 LEDs |
| Battery sense | **IO3**, 1:2 divider (wired on-board, nothing to solder) |
| Mic | none yet — see *Audio* below |

Charging is handled entirely by the board's charge IC. Plug in USB-C and it
charges; there is no firmware involved.

**Use a protected 18650 if you can.** The firmware cuts out at 3200mV and deep
sleeps, which covers the ESP32 — but the LED strip draws ~0.5–1mA per pixel even
while showing black, straight off the cell, and no amount of firmware stops
that. On an unprotected cell, pull the battery when you're done rather than
trusting the cutoff to hold overnight.

## Build and flash

```sh
pio run -t upload      # build + flash (auto-detects the port)
pio device monitor     # serial, 115200
./test/run.sh          # native tests, no hardware needed
```

If upload can't find the board: hold **BOOT**, tap **RST**, release BOOT, retry.

## Control app

`web/index.html` is the controller — a single self-contained page using Web
Bluetooth. It needs a browser that supports Web Bluetooth **and** a secure
context, which means Chrome/Edge on desktop or **Bluefy** on iOS. Safari and
Firefox will not work. Opening the file directly over `file://` will not work.

```sh
cd web && python3 -m http.server 8000
```

- Desktop Chrome: <http://localhost:8000>
- iPhone via Bluefy: `http://<your-laptop-ip>:8000`, same WiFi

Hit Connect and pick **Kwynumbra**.

Exposed over BLE: effect, brightness, three colors, BPM, and battery percent
(standard Battery Service, so a plain BLE scanner shows it too).

## Effects

`rainbow`, `chase`, `bass pulse`, `spectrum`. The last two are sound-reactive
and currently sit dark — see below.

## Audio

There is no microphone on the T-Energy S3. `sampleAudio()` feeds silence, so the
FFT runs on zeros and the two sound-reactive effects produce nothing. The band
analysis in `audio.cpp` is intact and still calibrated; attaching a mic means
rewriting `sampleAudio()` and nothing else.

Likely part: **INMP441** I2S MEMS mic on a long shielded lead, mic at the horns
and the board at the battery pack.
