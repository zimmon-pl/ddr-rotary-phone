# Wiring Map — RFT Serie 811 (VEB Fernmeldewerk Nordhausen, 1981)

Documented by visual inspection of PCB labels and multimeter continuity testing.

---

## PCB Terminal Reference

| Label | Meaning | Wire Color | ESP32? | Notes |
|-------|---------|------------|--------|-------|
| `NS1` | Nummernschalter 1 — dial impulse | brown | ✅ | |
| `NS2` | Nummernschalter 2 — dial contact | white | ✅ | |
| `NS3` | Nummernschalter 3 — dial ready (NSA) | green | ✅ | |
| `St` | Hook switch rail | no cable — solder new | ✅ | see hook switch section |
| `Mi` | Microphone | teal | ✅ | |
| `Fe1` | Fernsprecher 1 — earpiece/speaker | pink/red | ✅ | |
| `Fe2` | Fernsprecher 2 — earpiece/speaker | white | ✅ | |
| `WK` | Wecker Kontakt — ringer | — | ⬜ | leave untouched |
| `B2` | Bell 2 — ringer | — | ⬜ | leave untouched |
| `ET1` | Telephone line terminal 1 | — | ⬜ | leave untouched |
| `ET2` | Telephone line terminal 2 | — | ⬜ | leave untouched |
| `ET3` | Telephone line terminal 3 | — | ⬜ | leave untouched |
| `b1–b4` | Bell terminals | — | ⬜ | leave untouched |
| `a` / `b` | Telephone line (a/b pair) | — | ⬜ | used by hook switch |

---

## Hook Switch

Mechanism: transparent plastic spring-loaded switch mounted centrally on PCB. Contacts touch PCB traces directly.

| State | Continuity |
|-------|-----------|
| Handset on hook | `St` ↔ `ET2/ET3` |
| Handset lifted | `St` ↔ `a` + `b` |
| Black button pressed | `St` ↔ `ET1` |

**ESP32 connection (confirmed working 2026-04-19):**
- `St` → **GPIO 23** (with internal pull-up enabled in firmware)
- `a` (or `b`) → **GND**
- Logic: handset lifted → `St` shorts to GND → GPIO reads **LOW**. Handset down → pull-up wins → GPIO reads **HIGH**. (See [main/hook_switch.c](main/hook_switch.c) for the debouncing logic.)

Multimeter continuity confirmation (2026-04-24): `St ↔ Et2` pair closed when handset is on cradle. This matches the "on hook: St ↔ ET2/ET3" row in the state table above and is a useful alternative reference point if wiring from `a` isn't convenient.

---

## Rotary Dial

Three wires running from the dial mechanism into the phone: **green, brown, white** (confirmed visually when the phone was opened).

| Terminal | Function | Wire Color | ESP32 pin |
|----------|---------|------------|-----------|
| `NS1` | Dial pulses (NSI) — normally closed, opens on return stroke | brown | **GPIO 16** |
| `NS2` | Second dial contact | white | GND (return) |
| `NS3` | Dial active (NSA) — normally open, closes when dialing starts | green | **GPIO 17** |

Code in [main/rotary_dial.c](main/rotary_dial.c) is written (ISR + 50ms debounce + 2s number-complete timeout) but **not yet wired into `main.c` and not yet tested end-to-end on hardware** as of 2026-04-19.

**Multimeter-confirmed pairings (2026-04-24):** `brown ↔ green` = pulse pair (NSI, the one that clicks while the dial returns). `green ↔ white` = off-normal pair (NSA, closes while the dial is moved from rest). **Green is the common** — wire green to GND, brown to GPIO 16, white to GPIO 17.

---

## Handset

One coiled black cable runs from the handset into the phone. Conductors:

| Function | Wire Color |
|---------|------------|
| Speaker (+) | green |
| Shared GND (speaker return + mic return) | yellow |
| Microphone (+) | red |

Original speaker: HS-77 150Ω — replaced with a smaller 2W 8Ω unit.
Original microphone: FEP carbon capsule — being replaced with an electret capsule.

**Current state on the ESP32 side (2026-04-24):**
- Speaker path: temporary direct-solder connection from the handset speaker to the board — call audio is audible end-to-end. The permanent path is 3.5 mm jack → HPOUTL/HPOUTR.
- Microphone path: bias tee installed (2.2 kΩ resistor from mic node to 3.3 V, 10 µF electrolytic to LINEIN). Capsule is alive and producing a real AC signal at the jack (multimeter AC mV test: silent ≈ flat, voice/tap ≈ clear swing). **Blocked on a hardware bug in the A1S board itself — see the next section.**

---

## A1S V2.2 Microphone-Input Bug (BLOCKER for mic path)

**Discovered 2026-04-24.** The Ai-Thinker A1S V2.2 (PCB A541) has a documented PCB-layout bug: the two onboard MEMS microphone capsules (MIC1, MIC2) are AC-coupled through caps `C18` / `C20` to the **same ES8388 pins** (LIN2 / RIN2) that the external **LINEIN 3.5 mm jack** uses. External signals plugged into the jack arrive at the ADC already shorted by the onboard capsules. No software configuration — not input selection, not MICBIAS, not PGA gain, not the HPF — can route around a PCB trace.

Reference: [Phil Schatzmann — "The AI Thinker AudioKit Audio Input Bug" (2021)](https://www.pschatzmann.ch/home/2021/12/15/the-ai-thinker-audiokit-audio-input-bug/).

**Verified on this board:** external mic + bias tee produce AC signal at the jack (multimeter), but the ES8388 ADC reads a flat noise floor (~170 RMS idle, zero response to speech / taps) through the full range of codec register settings. Signal never reaches the ADC.

**Fix options (all require rework):**
1. Desolder the two onboard MIC capsules (MIC1, MIC2) — simplest, uses a regular iron, 10 minutes.
2. Move SMD caps `C18 → C17` and `C20 → C19` — Schatzmann's clean fix; routes onboard mics onto LIN1/RIN1 and frees LIN2/RIN2 for the jack. Requires 0402 SMD skills.
3. Bypass the A1S codec entirely for the mic path — add a discrete ADC (WM8782 or INMP441 breakout), feed its I2S into free ESP32 pins. This is the approach used by every other rotary-phone project I looked at (jtwaleson, derkorte, weeBell). Keeps the A1S for DAC duty.

---

## Bells

Two metal bells (RFT 805). Kept inside phone but **not connected to ESP32**.

Original ringer voltage: ~60V AC from telephone line — requires separate power supply, out of project scope.

Bell components (leave untouched):
- Copper electromagnet coil
- Capacitor (silver cylinder)
- Terminals `WK`, `B2`, `b1`–`b4`

---

## Black Button (Erdtaste)

Originally used to signal office PBX switchboards. Shorts `ET1` ↔ `ET3` when pressed. **Not used in this project.**

---

## ESP32 Connection Summary

| Function | Terminal A | Terminal B | ESP32 GPIO | Signal Type | Status |
|---------|-----------|-----------|------------|-------------|--------|
| Hook switch | `St` | `a` | **23** (pull-up) | Digital input | Working |
| Dial — pulses | `NS1` | `NS2` | **16** | Digital input (pulse count) | Wired, firmware not yet active |
| Dial — active | `NS3` | GND | **17** | Digital input | Wired, firmware not yet active |
| Microphone | handset red wire | handset yellow (GND) | LINEIN jack (via bias tee) → ES8388 LIN2/RIN2 | I2S in | **Blocked on A1S hardware bug** (see section above) |
| Speaker | handset green wire | handset yellow (GND) | HPOUTL/HPOUTR via 3.5mm jack (final); temp direct-solder today | I2S out | Working (temporary wiring) |
