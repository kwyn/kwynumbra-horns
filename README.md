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

<https://horns.kwyn.io/web/>

Served by GitHub Pages off `main`, so deploying is just a push. HTTPS is
enforced, and plain HTTP 301s up — which matters, because Web Bluetooth fails
silently on an insecure origin rather than telling you why.

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

### Hosting

GitHub Pages, `main` at root, custom domain `horns.kwyn.io` (a `CNAME` to
`kwyn.github.io` in DigitalOcean DNS, TTL 300).

The custom domain on the *project* repo is doing real work. Without it, project
pages inherit the `kwyn.github.io` user-site domain and 301 to `www.kwyn.io`,
which points at an unrelated DigitalOcean host — so the plain
`kwyn.github.io/kwynumbra-horns/` path does not serve this. Don't remove the
custom domain expecting to fall back to it.

Pages also gets the content types right, which a raw-file CDN may not: a service
worker served as `text/plain` refuses to register.

## Effects

`rainbow`, `chase`, `bass pulse`, `spectrum`, `bass bloom`. The last three are
sound-reactive and need the phone listening — see below.

`bass bloom` is the one built for bass music. The base of a horn is LED 0; each
kick throws a pulse out toward the tip, sustained sub sets a glow underneath it,
and the spectral balance of the track slides the colour along your chosen
palette over several seconds.

## Audio

There is no microphone on the board, and there is not going to be one. **The
phone is the microphone.** It is already paired to drive the horns, and the
browser has a hardware-accelerated FFT sitting idle; the ESP32 has neither a
double-precision FPU nor spare milliseconds in a 60fps render loop.

Tap **Listen** in the controller. The phone analyses the room and streams four
bytes to the horns — kick, bass, mid, high — around 25 times a second, and
detects tempo so the BPM-driven effects follow the track.

Two things to know:

- It only runs while the page is in the foreground. iOS suspends both the mic
  and Web Bluetooth when the page backgrounds or the phone locks, so pocket
  mode is out. The horns fade to their unlit state within half a second rather
  than freezing on whatever they were showing.
- The mic is requested with auto-gain, echo cancellation and noise suppression
  **off**. Those defaults are tuned for speech and actively fight bass content.

## Horn length

`Horn length` in the controller sets how many LEDs from the base make up one
horn, and `bass bloom` renders exactly that far. It is adjustable live because
the strip has never been counted — drag it up, see where the light stops
against the real tip, and that is your number.
