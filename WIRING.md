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

---

## Rotary Dial

Three wires running from the dial mechanism into the phone: **green, brown, white** (confirmed visually when the phone was opened).

| Terminal | Function | Wire Color | ESP32 pin |
|----------|---------|------------|-----------|
| `NS1` | Dial pulses (NSI) — normally closed, opens on return stroke | brown | **GPIO 16** |
| `NS2` | Second dial contact | white | GND (return) |
| `NS3` | Dial active (NSA) — normally open, closes when dialing starts | green | **GPIO 17** |

Code in [main/rotary_dial.c](main/rotary_dial.c) is written (ISR + 50ms debounce + 2s number-complete timeout) but **not yet wired into `main.c` and not yet tested end-to-end on hardware** as of 2026-04-19.

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

**Current state on the ESP32 side (2026-04-19):**
- Speaker path: temporary direct-solder connection from the handset speaker to the board — call audio is audible end-to-end. The permanent path is 3.5mm jack → HPOUTL/HPOUTR. **Jack plugs arrive 2026-04-20**; rewire then.
- Microphone path: **not installed yet.** Plan is electret capsule → passive bias tee (2.2kΩ + 10µF) → 3.5mm jack → LINEIN on the A1S. Bias tee components + jack plugs **arrive 2026-04-20**.

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
| Microphone | handset red wire | handset yellow (GND) | LINEIN (via bias tee + 3.5mm jack) | I2S in | Parts arrive 2026-04-20 |
| Speaker | handset green wire | handset yellow (GND) | HPOUTL/HPOUTR via 3.5mm jack (final); temp direct-solder today | I2S out | Working (temporary wiring) |
