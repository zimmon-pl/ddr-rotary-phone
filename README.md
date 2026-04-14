# FestStefan

**Giving new life to a 1981 DDR rotary phone.**

The vision: pick up the handset and talk to an AI. Spin the rotary dial and call someone through your mobile phone. When someone calls you, the phone rings — and you answer by lifting the handset. WhatsApp, FaceTime, regular calls, or just a conversation with Gemini. All through a phone that was made in East Germany nine years before I was born.

This is a work in progress. So far the ESP32 board plays audio through the headphone jack and pairs with a phone via Bluetooth as "FestStefan". Call audio routing is next.

### Built without engineering skills

I'm a product manager, not a developer. I don't know C. I've never soldered anything before this project. I bought a soldering iron, a multimeter, an ESP32 board, and this old phone from Kleinanzeigen for twenty euros. I installed VS Code and Claude Code, and started describing what I wanted to build.

Every line of firmware in this repo is being written through conversation — me describing what should happen, Claude writing the code, and both of us debugging when the headphones stay silent. The [project diary](DIARY.md) has the full story: the wrong codec guesses, the Wokwi simulator battles, the moment the first tone came through the headphones.

If you have an old phone gathering dust and some curiosity, follow along. No CS degree required.

## Hardware

| Component | Detail |
|---|---|
| Phone | DDR RFT Typ 550-14012, VEB Fernmeldewerk Nordhausen, 1981 |
| Board | ESP32 Audio Kit V2.2 (Ai-Thinker A541) |
| Codec | ES8388 at I2C 0x10 (confirmed by scan) |
| Audio output | 3.5mm headphone jack (MVP), handset speaker (final) |
| Connectivity | Bluetooth HFP (Phase 1), Wi-Fi + Gemini AI (Phase 2) |
| Power | 5V USB via Micro USB |

## How It Works

```
Lift handset ──► Dial tone plays
                 │
                 ├── Dial a number ──► Call connects via Bluetooth HFP
                 │
                 └── Wait 3 seconds ──► Gemini AI voice assistant (Phase 2)

Incoming call ──► Phone rings + LED flashes ──► Lift handset to answer

Replace handset ──► Call ends
```

## Roadmap

### Phase 1: Bluetooth Gateway (current)

- [x] ES8388 codec init + I2S audio — tone test confirmed
- [x] Bluetooth HFP pairing as "FestStefan" — connected to Pixel 8 Pro
- [ ] Call audio routed to headphone jack
- [ ] Simulation mode using onboard keys (KEY1-KEY6)
- [ ] State machine: IDLE / RINGING / DIALING / IN_CALL
- [ ] Green LED status patterns
- [ ] Hook switch integration (GPIO 4)
- [ ] Rotary dial pulse counting (GPIO 16/17)

### Phase 2: AI Assistant

- [ ] Wi-Fi connection manager
- [ ] Gemini Live API via WebSocket
- [ ] Voice-to-voice conversation through handset
- [ ] Smart home control via natural language

## Pin Map

### Audio (ES8388)

| Signal | GPIO |
|---|---|
| I2C SDA | 33 |
| I2C SCL | 32 |
| I2S MCLK | 0 |
| I2S BCK | 27 |
| I2S WS | 25 |
| I2S DOUT | 26 |
| I2S DIN | 35 |
| PA enable | 21 |

### Phone Interface

| Signal | GPIO |
|---|---|
| Hook switch | 4 |
| Rotary pulse (nsi) | 16 |
| Rotary off-normal (nsr) | 17 |

### Onboard Keys (simulation mode)

| Key | GPIO | Function |
|---|---|---|
| KEY1 | 36 | Simulate incoming call |
| KEY2 | 13 | Toggle hook (lift/replace) |
| KEY3 | 19 | Rotary pulse |
| KEY4 | 23 | Send dialed number |
| KEY5 | 18 | Volume up |
| KEY6 | 5 | Volume down |

## What You Need

**The phone:** Any rotary phone with a hook switch and pulse dial. Ours is a DDR RFT Typ 550 from 1981, bought on Kleinanzeigen Berlin for €20.

**Electronics:**
- ESP32 Audio Kit V2.2 (Ai-Thinker A1S) — ~€29
- Electret condenser microphone capsule 6mm — for handset mic
- 2W 8Ω mini speakers — for handset earpiece
- Dupont wire kit (M-F, M-M, F-F) — for prototyping connections
- WS2812B LED ring — for dial cavity lighting (size TBD)

**Tools:**
- Soldering iron (budget 80W set works fine)
- Digital multimeter
- Lead-free solder 0.6mm
- Flux + desoldering braid
- Third hand with magnifying glass
- Heat shrink tube set

**Software:**
- VS Code + Claude Code
- ESP-IDF v6.0

Total hardware spend: ~€160 including the phone. Full shopping list with links in [DIARY.md](DIARY.md).

## Building

Requires [ESP-IDF v6.0](https://docs.espressif.com/projects/esp-idf/en/v6.0/).

```bash
./build.sh build    # compile
./build.sh flash    # compile + flash
./build.sh monitor  # serial log output
./build.sh all      # build + flash + monitor
```

## Project Structure

```
main/
├── main.c           — app entry point, init sequence
├── audio.c / .h     — ES8388 codec + I2S + tone generation
├── bluetooth.c      — HFP: pairing, answer, hang up, dial
├── hook_switch.c/.h — handset lift/replace detection
├── rotary_dial.c/.h — pulse counting, digit decoding, timeout
├── keys.c           — onboard button input (planned)
├── state_machine.c  — IDLE/RING/DIAL/CALL logic (planned)
├── led.c            — green LED status patterns
├── dial_actions.c   — number to action routing
└── tones.c          — dial tone generation
```

## References

- [Bakelite to the Future](https://github.com/jtwaleson/bakelite-to-the-future-esp32) — Jouke Waleson's rotary phone project (logic reference)
- [Ai-Thinker A1S SDK](https://github.com/Ai-Thinker-Open/ESP32-A1S-AudioKit) — board SDK and codec drivers
- [ESP-IDF HFP HF example](https://github.com/espressif/esp-idf/tree/master/examples/bluetooth/bluedroid/classic_bt/hfp_hf) — Bluetooth hands-free reference

## License

MIT
