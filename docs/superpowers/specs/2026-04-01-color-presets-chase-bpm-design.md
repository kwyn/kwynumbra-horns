# Color Presets, Chase Effect & BPM Control

**Date:** 2026-04-01
**Status:** Design approved, pending implementation

## Problem

The current Kwynumbra firmware has 7 effects, most of which Kwyn won't use while DJing. Colors are hardcoded in the firmware with no way to change them at runtime. There's no tempo control, so animated patterns can't sync to the music being mixed.

## Goals

1. Streamline to 4 useful effects (cut Fire, Pulse, Sparkle, Solid)
2. Add user-selectable colors via 3 presets + custom color pickers
3. Add BPM control so Rainbow and Chase sync to the beat
4. Add a Chase effect that races 3 colors along the spiral horns
5. Keep the UI one-handed and glanceable for dusty playa conditions

## Non-Goals

- Tap tempo (user prefers manual BPM input)
- Persistent settings across power cycles
- More than 3 colors per palette

---

## Effects

4 effects total. All color-aware effects use 3 user-selected colors.

| Index | Name | Color Source | Speed Control |
|---|---|---|---|
| 0 | Rainbow | HSV cycle | BPM — controls hue advance rate |
| 1 | Chase | 3 user colors | BPM — controls chase advance rate |
| 2 | Bass Pulse | Color 1 (base color) | Sound-reactive (PDM mic) |
| 3 | Spectrum | 3 user colors | Sound-reactive (PDM mic) |

### Rainbow
HSV rainbow cycle across the strip. BPM controls how fast the hue rotates. At 128 BPM, one beat = ~469ms. Each beat advances the hue offset by a fixed chunk (tunable, start with 8 hue units per beat).

### Chase
3 colored segments, each ~1/3 of the strip, advancing along the LEDs. Creates a racing/spiral effect on the curling horns. BPM controls advance speed — one full cycle (all 3 colors completing one rotation) takes N beats (start with 4 beats per full cycle). Segments have soft edges using `fadeToBlackBy` for smooth transitions.

### Bass Pulse
Whole strip pulses with kick drum energy. Uses `getColor1()` as base color. Blends toward white as energy increases. Fast attack, slow decay smoothing. Unchanged from current behavior except color source.

### Spectrum
3 frequency zones mapped to strip sections:
- Horn base (LEDs 0–19): sub-bass → `getColor1()`
- Mid horn (LEDs 20–39): kick/bass → `getColor2()`
- Horn tips (LEDs 40–59): mids/highs → `getColor3()`

Brightness of each zone driven by its frequency band energy. Same audio analysis as current implementation.

---

## Color System

### Presets
3 hardcoded presets in the web UI. Tapping a preset writes all 3 RGB color values over BLE. Firmware has no concept of "presets" — it only sees 3 color values.

| Preset | Color 1 | Color 2 | Color 3 |
|---|---|---|---|
| Trans | `#F5A9B8` (pink) | `#5BCEFA` (light blue) | `#FFFFFF` (white) |
| Cyber | `#9B59B6` (purple) | `#00CED1` (cyan) | `#FF69B4` (hot pink) |
| Fire | `#FF0000` (red) | `#FF8C00` (orange) | `#FFD700` (gold) |

### Custom Colors
When "Custom" is selected, 3 color circles expand inline below the preset row. Each circle is an `<input type="color">` that opens the native iOS color picker. Changing a picker immediately writes that color's RGB over BLE. Selecting a preset collapses the custom pickers.

### Default Colors on Boot
Firmware defaults to Cyber preset colors (`CRGB(155,89,182)`, `CRGB(0,206,209)`, `CRGB(255,105,180)`) so the strip looks good before connecting.

---

## BPM Control

- Range: 60–200 BPM (uint8 over BLE, clamped in firmware callback)
- Default: 128 BPM
- UI: +/- stepper buttons flanking a large BPM number display
- Firmware converts BPM to milliseconds-per-beat: `60000 / bpm`
- Rainbow uses beat period to control hue advance timing
- Chase uses beat period to control position advance timing

---

## BLE Protocol

Single service UUID `19b10000-e8f2-537e-4f6c-d104768a1214`, 6 characteristics:

| Characteristic | UUID | Type | Description |
|---|---|---|---|
| Effect | `19b10001-e8f2-537e-4f6c-d104768a1214` | uint8 R/W | Effect index 0-3 |
| Brightness | `19b10002-e8f2-537e-4f6c-d104768a1214` | uint8 R/W | 0-128 |
| Color 1 | `19b10003-e8f2-537e-4f6c-d104768a1214` | 3 bytes R/W | RGB |
| Color 2 | `19b10004-e8f2-537e-4f6c-d104768a1214` | 3 bytes R/W | RGB |
| Color 3 | `19b10005-e8f2-537e-4f6c-d104768a1214` | 3 bytes R/W | RGB |
| BPM | `19b10006-e8f2-537e-4f6c-d104768a1214` | uint8 R/W | 60-200 |

Color characteristics: web UI writes `new Uint8Array([r, g, b])`. Firmware reads 3 bytes via `getValue()` and constructs `CRGB(r, g, b)`.

---

## Web UI Layout

Mobile-first, 320px wide target (iPhone in Bluefy). Dark theme, purple accent. No scrolling needed.

```
┌──────────────────────────┐
│       KWYNUMBRA          │
│   [ Connect Button ]     │
│                          │
│  Brightness  ━━━●━━━ 100 │
│                          │
│  BPM     [−]  128  [+]  │
│                          │
│  ●●● Trans  ●●● Cyber   │
│  ●●● Fire   ●●● Custom  │
│  (if Custom: 3 color     │
│   picker circles expand) │
│                          │
│  ┌────────┐ ┌────────┐  │
│  │   🌈   │ │   🏁   │  │
│  │Rainbow │ │ Chase  │  │
│  └────────┘ └────────┘  │
│  ┌────────┐ ┌────────┐  │
│  │   🥁   │ │   🎵   │  │
│  │  Bass  │ │Spectrum│  │
│  │ Pulse  │ │        │  │
│  └────────┘ └────────┘  │
└──────────────────────────┘
```

### Interaction Details
- **Preset buttons**: 2x2 grid. Each shows 3 color dots + label. Active preset highlighted with purple border/glow. Tapping writes all 3 colors over BLE.
- **Custom expand**: When Custom is active, a row of 3 large colored circles appears below presets. Tap a circle → native color picker. Selecting any preset collapses the custom row.
- **BPM stepper**: `[−]` and `[+]` buttons, large BPM number between them. Each tap changes by 1. No long-press acceleration needed (simple is fine).
- **Effect buttons**: 2x2 grid with emoji + label. Active effect has purple border/glow.
- **On connect**: Read all 6 characteristics to sync UI state.
- **On disconnect**: Show reconnect button, hide controls (existing behavior).

---

## Files to Modify

| File | Changes |
|---|---|
| `src/config.h` | Update effect constants (4 effects), add color/BPM UUIDs, add DEFAULT_BPM |
| `src/ble_control.h` | Add getters: `getColor1()`, `getColor2()`, `getColor3()`, `getBPM()` |
| `src/ble_control.cpp` | Add color/BPM state, characteristics, and callbacks |
| `src/effects.cpp` | Remove 4 effects, add Chase, update Rainbow/BassPulse/Spectrum to use colors+BPM |
| `web/index.html` | Rebuild UI: 4 effects, presets, custom pickers, BPM stepper |

Files with no changes: `src/effects.h`, `src/main.cpp`, `src/audio.h`, `src/audio.cpp`, `platformio.ini`

---

## Verification

1. Flash firmware → default rainbow plays at 128 BPM speed
2. Connect from Bluefy → UI shows 4 effects, 4 presets, brightness slider, BPM stepper
3. Tap Trans preset → preset highlights, color dots update
4. Tap Chase → 3-color segments race along strip at 128 BPM
5. Change BPM to 80 → Chase and Rainbow both slow down
6. Change BPM to 170 → both speed up
7. Tap Custom → 3 color picker circles appear
8. Pick custom colors → strip updates immediately
9. Tap Cyber preset → custom pickers collapse, colors change
10. Tap Bass Pulse near music → strip pulses with user-selected colors
11. Tap Spectrum near music → 3 zones light up with user-selected colors
12. Disconnect → reconnect → device re-advertises, UI syncs state
