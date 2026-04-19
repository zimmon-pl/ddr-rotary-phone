# DDR Rotary Phone Bluetooth Headset — ESP32 Project

## Project Overview

Convert a 1981 DDR RFT rotary phone (VEB Fernmeldewerk Nordhausen, Typ 550-14012) into a fully functional Bluetooth headset using an ESP32 Audio Kit A1S board. The phone pairs with a smartphone via HFP (Hands-Free Profile) and works with any call — regular cellular, WhatsApp, or any other app that uses the system audio.

---

## Hardware (CONFIRMED 2026-04-14)

- **Board:** ESP32 Audio Kit A1S (Ai-Thinker), PCB revision **A541**, module **K536**, chip rev **v3.1**
- **Codec:** ES8388 at I2C address 0x10 — **Variant 1 (Board 5)** confirmed by I2C scan
- **Phone:** DDR RFT rotary phone Typ 550, 1981
- **LED:** WS2812B LED ring (size TBD — measure after opening phone)
- **Power:** 5V USB via Micro USB cable
- **Mounting:** Velcro pads inside phone base
- **Serial port:** `/dev/cu.usbserial-0001`

---

## Firmware Stack

- **ESP-IDF v6.0** (not Arduino) — path: `~/.espressif/v6.0/esp-idf`
- **ESP-ADF** installed at `~/esp/esp-adf` (reference only — not using its build system)
- **Codec driver:** Direct ES8388 I2C register writes in `main/audio.c` (no external component needed — **confirmed working 2026-04-14**)
- **Logic reference:** `jtwaleson/bakelite-to-the-future-esp32` — different hardware, logic reference only
- **Build/Flash/Monitor:** Use `./build.sh build|flash|monitor|all`
- **Do NOT** manually manage Python venvs or run `pip install`. The build script handles all paths.
- **Do NOT** suggest `idf.py add-dependency` for the codec — our direct driver works.

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

## Current Firmware State

**Single source of truth for "what works" is the Phase 1 checklist in [README.md](README.md).** Update the checkboxes there when behaviour changes — do not duplicate status in this file.

**For physical/solder-level wiring** (PCB terminals, handset cable colours, hook-switch continuity, which bell terminals not to touch) see [WIRING.md](WIRING.md). Consult it before suggesting any hardware change.

What a new session needs to know without greping:

- State routing lives as the `on_hook_change` callback in [main/main.c](main/main.c). Lift → answer if `bluetooth_is_ringing()`, else `audio_start_dial_tone()`. Replace → stop dial tone + hangup if ringing/in-call. No separate `state_machine.c`.
- [main/audio.c](main/audio.c) (~620 lines) owns everything audio: ES8388 init, I2S, tone playback, dial tone task, call audio RX task, mic capture TX task, volume/mute/mic-gain.
- [main/bluetooth.c](main/bluetooth.c) (~280 lines) owns HFP: GAP, `hf_client_callback`, `hf_incoming_data_cb`/`hf_outgoing_data_cb`, plus `bluetooth_answer_call`, `bluetooth_hangup_call`, `bluetooth_is_ringing`, `bluetooth_is_in_call`.
- [main/hook_switch.c](main/hook_switch.c) — ISR on GPIO 23 + 50ms debounce, task fires `hook_change_cb_t` callback.
- [main/rotary_dial.c](main/rotary_dial.c) — pulse counting + 2s number-complete timer. **Module is written but NOT yet called from `main.c`** — no `rotary_dial_init()` in app_main.
- [main/dial_actions.c](main/dial_actions.c), [main/led.c](main/led.c), [main/tones.c](main/tones.c) — 11-line stubs, just an `_init()` that logs. Real dial tone lives in `audio.c`, not `tones.c`.

## File Structure (target — aspirational items still to come)

```
main/
├── main.c           — init + hook-switch callback (state routing)
├── bluetooth.c      — HFP: pairing, answer, hang up, dial
├── audio.c          — ES8388 codec config, I2S, mic/speaker, dial tone
├── rotary_dial.c    — pulse counting, digit decoding, 2s timeout
├── hook_switch.c    — lift/replace detection via GPIO 23
├── dial_actions.c   — number → action routing (stub today)
├── led.c            — WS2812B ring state machine (stub today)
└── tones.c          — stub; delete once we're sure nothing will move here
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

## GPIO Pin Assignments (CONFIRMED)

### ES8388 codec pins (DO NOT USE for anything else)
| Signal | GPIO |
|---|---|
| I2C SDA | 33 |
| I2C SCL | 32 |
| I2S MCLK | 0 |
| I2S BCK | 27 |
| I2S WS | 25 |
| I2S DOUT | 26 |
| I2S DIN | 35 (input only) |
| PA enable | 21 (must set HIGH for speaker output) |

### Other board pins (DO NOT USE)
| GPIO | Used by |
|---|---|
| 1 | UART TX |
| 3 | UART RX |
| 5 | Headphone detect |
| 13 | Button MODE / SD card |
| 22 | Green LED (on board) |
| 34 | SD card interrupt (input only) |
| 36 | Button REC (input only) |

### Our project pins
| Signal | GPIO |
|---|---|
| Hook switch | 23 |
| Rotary dial pulse (nsi) | 16 |
| Rotary dial off-normal (nsr) | 17 |
| WS2812B LED ring data | TBD (22 is green LED on board — maybe use 2 or 15) |

### Free GPIOs available
2, 4, 12, 14, 15, 16, 17, 18, 19, 23

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

- **Board is confirmed — do NOT re-scan or ask about revision.** It's A541/K536, ES8388 Variant 1, chip v3.1. All pins are documented above.
- **Always use ESP-IDF v6.0** from `~/.espressif/v6.0/esp-idf` — never fall back to v5.2.3
- Serial port: `/dev/cu.usbserial-0001`
- Flash: `python3 $IDF_PATH/tools/idf.py -p /dev/cu.usbserial-0001 flash`
- Monitor (from real terminal, not Claude Code): `python3 $IDF_PATH/tools/idf.py -p /dev/cu.usbserial-0001 monitor`
- To read serial from Claude Code, use pyserial (see I2C scanner session for example)
- GPIO 21 must be set HIGH to enable the onboard power amplifier for speaker output
- WS2812B ring size TBD — measure dial cavity before ordering ring
- Wokwi simulator: use `wokwi-esp32-devkit-v1` board type, start with "Wait for Debugger" mode
