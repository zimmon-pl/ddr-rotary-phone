# DDR Rotary Phone Bluetooth Headset — ESP32 Project

## Project Overview

Convert a 1981 DDR RFT rotary phone (VEB Fernmeldewerk Nordhausen, Typ 550-14012) into a fully functional Bluetooth headset using an ESP32 Audio Kit A1S board. The phone pairs with a smartphone via HFP (Hands-Free Profile) and works with any call — regular cellular, WhatsApp, or any other app that uses the system audio.

---

## Hardware

- **Board:** ESP32 Audio Kit A1S (Ai-Thinker, ES8388 codec)
- **Phone:** DDR RFT rotary phone Typ 550, 1981
- **LED:** WS2812B LED ring (size TBD — measure after opening phone)
- **Power:** 5V USB via Micro USB cable
- **Mounting:** Velcro pads inside phone base

---

## Firmware Stack

- **ESP-IDF** (not Arduino)
- **ESP-ADF** on top of ESP-IDF
- **Base SDK:** `Ai-Thinker-Open/ESP32-A1S-AudioKit`
- **Logic reference:** `jtwaleson/bakelite-to-the-future-esp32` — different hardware, so logic reference only; audio layer needs full adaptation for ES8388

---

## MVP Scope — Bluetooth Only

### Bluetooth HFP
- Pair with smartphone as HFP headset
- Auto-reconnect on power up
- Works with all calls: cellular, WhatsApp, FaceTime, etc.
- Outgoing calls use regular cellular — HFP does not support dialing via WhatsApp directly

### Hook Switch
- Lift handset → answer incoming call
- Replace handset → end call
- Lift handset with no incoming call → dial tone + ready to dial

### Rotary Dial
- Count pulses per digit
- 2-second silence after last digit = number complete
- Dial number via HFP AT command
- Replace handset mid-dial = cancel

### Dial Tone
- 440 Hz + 350 Hz sine wave mix played through handset speaker via ES8388
- Plays for ~2 seconds after lifting handset when no incoming call

### Incoming Call
- Ring signal through handset speaker
- LED ring pulses red

### Reset / Re-pair
- Lift handset → dial `999` → replace handset
- ESP32 restarts and re-pairs BT

### LED Ring (WS2812B)

| State | Color |
|---|---|
| BT searching | Slow blue breathing |
| BT connected, idle | Solid blue |
| Incoming call | Fast red pulse |
| Active call | Solid green |
| Dialing | Rotating white |
| No BT | Solid red |

---

## File Structure

```
main/
├── main.c           — init, BT startup
├── bluetooth.c      — HFP: pairing, answer, hang up, dial
├── audio.c          — ES8388 codec config, I2S, mic/speaker
├── rotary_dial.c    — pulse counting, digit decoding, 2s timeout
├── hook_switch.c    — lift/replace detection via GPIO
├── dial_actions.c   — number → action routing
├── led.c            — WS2812B ring, state machine
└── tones.c          — dial tone generation (sine wave mix)
```

---

## Key Logic

### Hook Switch States

```
Handset down = hook switch CLOSED (pressed by weight)
Handset up   = hook switch OPEN (spring returns)
```

### Priority / Flow When Handset Lifted

```
Lift handset
→ Play dial tone (1–2s)
→ LED: green

If user dials digits:
    → Count pulses, decode digits
    → 2s silence after last digit = number complete
    → HFP dial via AT+ATD (regular cellular call)

If user does NOT dial (V2 — WiFi):
    → After 3s silence: connect to Gemini Live API
    → LED: rainbow spin
    → Live voice conversation via WebSocket
```

Dial `999` = factory reset / BT re-pair

---

## GPIO Pin Assignments (TBD)

Depends on board revision. Once board arrives, identify revision (A210 or A237) from number printed on PCB underside and configure accordingly.

| Signal | GPIO |
|---|---|
| Hook switch | TBD |
| Rotary dial pulse | TBD |
| WS2812B data | TBD (GPIO22 likely free) |
| ES8388 config | Per revision `dac_config` |

---

## Testing Approach

### Phase 1 — Before Hardware Arrives (Mac Only)

**Environment setup:**
- Install VS Code + ESP-IDF extension (guided wizard)
- Clone `Ai-Thinker-Open/ESP32-A1S-AudioKit` and `jtwaleson/bakelite-to-the-future-esp32`
- Run `idf.py build` to verify toolchain works before touching hardware

**Wokwi simulator (https://wokwi.com):**
- Use for logic modules that don't require real audio hardware
- Test `rotary_dial.c` — simulate GPIO pulses, verify digit decoding and 2s timeout
- Test `hook_switch.c` — simulate open/close transitions, verify state changes
- Test `dial_actions.c` — verify number → action routing logic (999 reset, normal number, etc.)
- Test `led.c` — verify state machine transitions visually
- Cannot test in Wokwi: Bluetooth HFP, ES8388 audio codec, actual I2S audio

**Unit logic tests on Mac:**
- `rotary_dial.c` and `hook_switch.c` are pure GPIO logic — can be tested as standalone C with mock GPIO values

### Phase 2 — Hardware Arrives

Step-by-step flash and verify:

| Step | Test |
|---|---|
| 1. `idf.py build` | Compiles without errors |
| 2. `idf.py flash` | Board starts, LED does something |
| 3. BT pairing | Phone sees device, connects |
| 4. HFP audio | Make call, hear audio through handset |
| 5. Hook switch | Lift/replace triggers correct GPIO |
| 6. Dial tone | Tone plays after lifting handset |
| 7. Rotary dial | Digits decoded correctly (use serial monitor) |
| 8. End-to-end | Dial a number, call connects |
| 9. LED ring | Each state shows correct color |

**Serial monitor during testing:**
```bash
idf.py monitor
```

All modules should log key events (hook switch state, digit decoded, BT event, etc.) so debugging is possible without oscilloscope.

Reflashing: unlimited, ~30 seconds per flash via Micro USB.

---

## V2 Scope — WiFi + Gemini Live

- WiFi autoconnect (credentials in `menuconfig`)
- If WiFi available + handset lifted + no dialing after 3 seconds → connect Gemini Live API (WebSocket, streaming audio)
- Gemini handles smart home via natural language: "turn on the lights"
- Smart home devices: Shelly 1PM (lamp on 230V side), Tuya bulbs — Gemini calls their HTTP endpoints
- LED: rainbow spin during Gemini session
- Add `wifi.c`, `ai_assistant.c`, `smart_home.c` modules

---

## References

- Ai-Thinker A1S SDK: https://github.com/Ai-Thinker-Open/ESP32-A1S-AudioKit
- Jouke's rotary phone project (logic reference): https://github.com/jtwaleson/bakelite-to-the-future-esp32
- Blog post with full build details: https://blog.waleson.com/2024/10/bakelite-to-future-1950s-rotary-phone.html
- ESP-IDF HFP HF example: https://github.com/espressif/esp-idf/blob/master/examples/bluetooth/bluedroid/classic_bt/hfp_hf/README.md
- pschatzmann arduino-audiokit (ES8388 reference): https://github.com/pschatzmann/arduino-audiokit
- Wokwi ESP32 simulator: https://wokwi.com

---

## Notes for Claude Code

- Start with `idf.py build` to verify toolchain before touching hardware
- Board needs Micro USB cable — powers and flashes via same cable
- Flash command: `idf.py flash monitor`
- First flash may require holding `BOOT` button on board
- ES8388 `dac_config` varies by board revision — confirm revision after receiving hardware by photographing PCB underside
- WS2812B ring size TBD — measure dial cavity before ordering ring
- `rotary_dial.c` and `hook_switch.c` can be started immediately in Wokwi before hardware arrives
