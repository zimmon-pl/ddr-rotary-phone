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

**ESP32 connection:**
- Pin 1 → `St`
- Pin 2 → `a` (or `b`)
- Logic: continuity = handset lifted → GPIO HIGH

---

## Rotary Dial

Three wires running from dial mechanism to PCB.

| Terminal | Function | Wire Color |
|----------|---------|------------|
| `NS1` | Dial pulses (NSI) — normally closed, opens on return stroke | brown |
| `NS2` | Second dial contact | white |
| `NS3` | Dial active (NSA) — normally open, closes when dialing starts | green |

Exact color-to-terminal mapping to be confirmed when reconnecting.

---

## Handset

Coiled black cable, 4 wires.

| Function | Wire Color |
|---------|------------|
| Shared GND | yellow |
| Speaker (+) | green |
| Speaker (-) | yellow (paired with green) |
| Microphone | remaining wire(s) |

Original speaker: HS-77 150Ω — replaced with smaller unit, wired to LOUT+/- on ESP32 Audio Kit V2.2.
Original microphone: FEP carbon capsule — not connected, using onboard mic for now.

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

| Function | Terminal A | Terminal B | Signal Type |
|---------|-----------|-----------|-------------|
| Hook switch | `St` | `a` | Digital input |
| Dial — pulses | `NS1` | `NS2` | Digital input (pulse count) |
| Dial — active | `NS3` | GND | Digital input |
| Microphone | `Mi` | GND | Analog / I2S |
| Speaker | `Fe1` | `Fe2` | PWM / I2S |
