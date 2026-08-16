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
Bluetooth. That needs a browser which supports the API **and** a secure context:
Chrome/Edge on desktop, or **Bluefy** on iOS. Safari and Firefox don't support
Web Bluetooth at all, and `file://` won't work anywhere.

**On the phone**, load this in Bluefy:

<https://raw.githack.com/kwyn/kwynumbra-horns/main/web/index.html>

Use the `/main/` URL, not a commit-pinned one — the path has to stay stable or
the service worker cache resets on every deploy. githack's CDN cache clears
within a minute or two of a push.

**On desktop**, serve it locally (`localhost` counts as a secure context, so no
certificate needed):

```sh
cd web && python3 -m http.server 8000   # then http://localhost:8000
```

There is no LAN equivalent for the phone. Bluefy requires HTTPS, so
`http://<laptop-ip>:8000` is rejected — the hosted URL is the only phone route.

Hit Connect and pick **Kwynumbra**. Exposed over BLE: effect, brightness, three
colors, BPM, and battery percent (standard Battery Service, so a plain BLE
scanner shows it too).

### Offline

A service worker caches the page, so it works with no signal — **but only after
one successful online load.** Open it on WiFi and add it to the home screen
before heading anywhere without service. Verify by killing WiFi and cellular and
reopening it.

Hosting is irrelevant offline; the service worker is the whole story. It's
network-first, so an online load always gets the latest push and a cold offline
launch that was never cached gets nothing.

Note that colors, effects, BPM and brightness all travel over BLE — none of that
needs the page to be updated. Only UI changes require a fresh load.

### GitHub Pages (currently a dead end)

Pages is enabled on `main` at root, but `kwyn.github.io` 301-redirects to the
`www.kwyn.io` user-site custom domain, which resolves to a DigitalOcean host
rather than GitHub's `185.199.108–111.153`, and does not respond. Until that
host is fixed or the custom domain comes off the user site, use githack.

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
