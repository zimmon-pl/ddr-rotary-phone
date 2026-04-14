# Project Diary — DDR Rotary Phone Bluetooth Build

## The Spark

It started with a simple question: *can I make an old rotary phone work as a Bluetooth device so I can talk through WhatsApp?* What followed was a long, winding, genuinely fun conversation that went from a DIY electronics idea to a full project spec, a shopping list, a router audit, a Hikvision camera setup, and a smart home tour — before landing back on the phone.

---

## What I Bought

**The phone:**
- DDR RFT rotary phone, Typ 550-14012, VEB Fernmeldewerk Nordhausen, 1981 — bought on Kleinanzeigen Berlin (Moabit), €20, picking up Thursday 2026-04-17

**Electronics:**
- ESP32 Audio Kit A1S (Ai-Thinker, ES8388 codec) — ~€29, Amazon.de
- Shelly Plus 1PM WiFi relay (for the 12V lamp project) — ~€14, Amazon.de
- AstroAI AM33D digital multimeter — ~€16.58, Amazon.de
- KELLYSHUN KL-558 flux + desoldering braid set — ~€8.49, Amazon.de
- Lead-free solder 0.6mm, 20g — ~€5.79, Amazon.de
- Heat shrink tube set 328 pieces — ~€4.99, Amazon.de
- QWORK third hand with magnifying glass — ~€9.84, Amazon.de
- Self-adhesive tape strips 50x100mm pack of 20 — ~€5.99, Amazon.de
- XGMATT USB-C to Micro-USB cable 0.5m × 2 — Amazon.de
- Sourcing Map electret condenser microphone capsule 6mm × 3.5mm, pack of 3 — Amazon.de
- Weewooday 2W 8Ω mini metal shell speakers × 6 — Amazon.de
- Elegoo 120pcs Dupont wire kit (M-F, M-M, F-F) — Amazon.de
- UHU patafix transparent double-sided adhesive pads, pack of 56 — Amazon.de
- WS2812B LED ring — TBD (measure phone dial cavity first)
- 80W soldering iron set — ~€21.99, Amazon.de (budget option, sufficient for 6 solder joints)
- USB wall charger for ESP32 power — using existing charger from home

**Total hardware spend:** ~€160 including the phone

---

## What Tools We Used During This Chat

- **Gmail MCP** — retrieved order history to confirm the BA15S 12V lamp spec, which led to the Shelly Plus 1PM decision
- **Web search** — found Amazon.de product links, checked Shelly/Sonoff prices, researched ESP32 Audio Kit variants, found Jouke's "Bakelite to the Future" project

---

## What We Considered

**Hardware path for the phone:**
- Simple Bluetooth AUX adapter (no soldering, just plug) — cheapest, least elegant
- ESP32 Audio Kit A1S with ESP-ADF + HFP — full DIY, most capable, chosen path
- Rotary dial as smart home trigger — decided yes, included in V2

**Soldering iron:**
- TS101 (~€65) — best quality, dismissed as overkill for 6 joints
- Pinecil V2 (~€70 in Germany, not €30 as expected) — dismissed
- Budget 80W set (~€22) — chosen for MVP, sufficient
- Ruthex — turned out to be a 3D printing insert brand, not a soldering iron

**Smart home for lamp:**
- Shelly 1 Mini Gen3 (Matter, cheapest) — out of stock for a month
- Sonoff Mini R2 (no Matter, new app) — dismissed
- Shelly Plus 1PM — chosen, already in basket, has power monitoring as bonus

---

## My Thinking

The project evolved in layers. It started as "make old phone work with WhatsApp" and grew into something more interesting: a device with its own personality, where the rotary dial is a physical interface to both the phone network and the smart home.

The key insight was the **priority logic**: Bluetooth always wins when you dial a number. WiFi/Gemini only activates when you lift the handset and wait — 3 seconds of silence means you want to talk to an AI, not dial a person. This makes the interaction feel natural rather than mode-switched.

The decision to use **Gemini Live instead of hardcoded smart home shortcuts** was a late but good one — saying "turn on the lights" into a 1981 rotary phone is a better experience than dialing `1` for the lamp, and it handles any command without pre-programming every scenario.

The **LED ring** was added as visual feedback but became a genuine design element — the states (breathing blue, pulsing red, rainbow spin) give the phone a character it wouldn't have otherwise.

---

## Steps Taken

1. Researched Bluetooth approaches for rotary phone — settled on ESP32 Audio Kit A1S with ESP-ADF
2. Found Jouke Waleson's "Bakelite to the Future" project as the main reference
3. Built out the full shopping list across amazon.de and Allegro
4. Identified DDR RFT Typ 550 phone on Kleinanzeigen Berlin — €20, Moabit
5. Researched soldering tools, safety, and beginner YouTube resources
6. Identified BA15S 12V lamp via Gmail — chose Shelly Plus 1PM on 230V side as smart switch
7. Designed full MVP user journey and V2 scope
8. Decided on Gemini Live as V2 AI interface instead of hardcoded smart home shortcuts
9. Wrote complete project spec document ready for Cowork + Claude Code
10. ESP32 Audio Kit A1S ordered and delivered 2026-04-14
11. VS Code + ESP-IDF already set up and ready

---

## Development Log

### 2026-04-13 — Project setup
- Set up the ESP-IDF project structure with all module stubs
- Created GitHub repo: https://github.com/aaandekk/ddr-rotary-phone
- First clean build — firmware compiles with BT Classic + HFP enabled
- Installed ESP-IDF v5.2.3 toolchain on Mac

### 2026-04-13 (evening) — Wokwi simulator battle
Long session getting the Wokwi VS Code extension to work. The simulator kept showing a blank terminal — no serial output at all despite the board rendering visually.

**What we tried (and failed):**
- Different board types (`board-esp32-devkit-c-v4` — didn't work)
- Various pin name formats for serial (`TX`/`RX`, `TX0`/`RX0`)
- Custom `wokwi-flasher.json` with `build/` prefixed paths
- Symlinks and file copies for firmware files
- Adding `fullBoot`, `builder` attributes
- Merged binary firmware approach
- Multiple sdkconfig changes (2MB → 4MB flash, BT disabled)

**What actually fixed it:**
1. **Board type must be `wokwi-esp32-devkit-v1`** (not `board-esp32-devkit-c-v4`)
2. **Serial connections:** `["esp:TX0", "$serialMonitor:RX", "", []]` — discovered by comparing with the official Wokwi example project (`wokwi/esp32-idf-hello-wifi`)
3. **License activation:** "Wokwi: Request a New License" — without this, the board renders but firmware doesn't execute
4. **Start mode:** "Start Simulator and Wait for Debugger" with `gdbServerPort = 3333`

**Also done:**
- Upgraded from ESP-IDF v5.2.3 to v6.0 — installed Python environment, clean build works
- Cloned and analyzed Jouke's reference project (`jtwaleson/bakelite-to-the-future-esp32`) — extracted his GPIO pin assignments and rotary dial logic
- Cloned and analyzed Ai-Thinker A1S SDK — mapped all taken/free GPIOs on the AudioKit board
- Changed GPIO assignments from 12/14/15 (conflict with A1S SD/JTAG pins) to 4/16/17 (free pins)
- Wrote full hook_switch.c and rotary_dial.c with interrupts, debouncing, pulse counting, 2s timeout
- Fixed ISR crash in hook_switch.c — moved esp_timer calls out of ISR, using queue + task-based debounce instead

**Unresolved:**
- Only GPIO4 (`esp:D4`) works in Wokwi wiring — GPIO 2/15/16/17 connections don't show up visually and don't work. Need to figure out correct Wokwi pin names for these GPIOs. The hook switch logic is verified working, but rotary dial can't be tested yet.

---

### 2026-04-14 — Board arrived, first audio confirmed!

The ESP32 Audio Kit arrived. Plugged it in, ran the I2C scanner from the previous session — confirmed **ES8388 codec at address 0x10** (not AC101, despite what the generic V2.2 specs say). Gemini suggested AC101 based on the board revision label, but the actual scan is the ground truth.

**Hardware confirmed:**
- Board: ESP32 Audio Kit V2.2, Ai-Thinker A541, chip rev v3.1
- Codec: ES8388 at I2C 0x10
- I2C: SDA=33, SCL=32
- I2S: MCLK=0, BCK=27, WS=25, DOUT=26, DIN=35
- PA enable: GPIO 21 (must be HIGH for audio)
- Green LED: GPIO 22
- 6 tactile keys: KEY1-KEY6 on GPIOs 36, 13, 19, 23, 18, 5
- Serial port: `/dev/cu.usbserial-0001`

**What we built:**
- Wrote full ES8388 driver in `audio.c` — direct I2C register writes, no external component needed
- I2S standard mode: 44100Hz, 16-bit stereo, ESP32 as master
- Tone test: 440Hz (2s) then 880Hz (1s) through 3.5mm headphone jack
- **Both tones audible through airline headphones — audio layer confirmed working!**
- Created `build.sh` helper script to avoid ESP-IDF environment PATH issues
- Added `.claudeignore` to keep build artifacts out of Claude's context

**Design decisions:**
- BT device name: **"FestStefan"**
- Simulation mode planned: use KEY1-KEY6 to simulate hook switch, rotary dial, and incoming calls before the physical phone arrives
- State machine: IDLE → RINGING → IN_CALL, and IDLE → DIALING → IN_CALL
- Green LED (GPIO 22) will show state via blink patterns (slow=searching, solid=connected, fast=ringing)
- GPIO 19 used as KEY3 button, so blue LED unavailable

**What we learned:**
- Ai-Thinker swaps codecs between production runs — always I2C scan first, never trust generic specs
- ESP-IDF `export.sh` doesn't work in Claude Code's shell (env vars don't persist across `&&` chains) — `build.sh` with hardcoded paths is the workaround
- The I2S DMA keeps looping the last buffer after playback ends — must mute the DAC explicitly

---

### 2026-04-14 (evening) — Bluetooth pairing works!

Wrote `bluetooth.c` with full HFP Hands-Free client init. Flashed, opened Bluetooth settings on Pixel 8 Pro, and "FestStefan" appeared immediately. Paired via SSP (Secure Simple Pairing), Service Level Connection established.

**What the logs showed:**
- Authentication success with "And. Pixel 8 Pro"
- SLC connected — all HFP indicators flowing: signal strength, battery level, network status, call state
- In-band ring tone negotiation happened automatically
- BT address: `e0:8c:fe:64:42:2e`

**Key decisions:**
- SSP with IO capability "None" (just-works pairing, no PIN prompt) — simplest for a headset device
- Also set legacy PIN "0000" as fallback for older phones
- Device name "FestStefan" confirmed visible and pairable

**Binary size:** 673KB with Bluetooth stack (was 189KB without). 36% of flash partition still free.

### 2026-04-14 (evening) — Bluetooth pairing works! 

Making a phone call as well. Logs below. I ran out of credits on Claude, so this is everything for today, and I could even barely hear some voice when I called someone. It was just very quiet, so we probably need to modify a little bit the volume. We can start from this next time. 

W (170649) BT_HCI: hcif mode change: hdl 0x80, mode 0, intv 0, status 0x0
I (170659) bluetooth: Power mode changed: 0
I (170679) bluetooth: Call setup: outgoing_dialing
I (170679) bluetooth: Call setup: outgoing_alerting
I (170699) bluetooth: Audio: connected_msbc
I (170699) audio: Starting call audio at 16000 Hz
I (170699) audio: DAC unmuted
I (170699) audio: Call audio started
I (170699) audio: Call audio task started
I (171549) bluetooth: Volume speaker: 5
I (171699) bluetooth: Call indicator: CALL IN PROGRESS
I (171759) bluetooth: Call setup: none
I (176079) bluetooth: Volume speaker: 6
I (176809) bluetooth: Volume speaker: 5
I (176869) bluetooth: Volume speaker: 6
I (176939) bluetooth: Volume speaker: 7
I (176979) bluetooth: Volume speaker: 8
I (177069) bluetooth: Volume speaker: 9
I (177329) bluetooth: Volume speaker: 10
I (178189) bluetooth: Volume speaker: 11
I (178449) bluetooth: Volume speaker: 12
I (178499) bluetooth: Volume speaker: 13
I (178559) bluetooth: Volume speaker: 14
I (178559) bluetooth: Volume speaker: 15
W (185759) BT_HCI: hci cmd send: sniff: hdl 0x80, intv(400 800)
W (185759) BT_HCI: hcif mode change: hdl 0x80, mode 0, intv 0, status 0x20
E (185759) BT_APPL: bta_dm_pm_btm_status hci_status=32
I (185769) bluetooth: Power mode changed: 0
W (223769) BT_HCI: hcif disc complete: hdl 0x180, rsn 0x13 dev_find 1
I (223769) bluetooth: Audio: disconnected
I (223769) audio: Stopping call audio
I (223809) audio: Call audio task ended
I (223869) audio: DAC muted
I (223869) audio: Call audio stopped, I2S back to 44100 Hz
I (223869) bluetooth: Call indicator: no call
I (224169) bluetooth: Signal strength: 5

These locks should be removed once Claude updates the diary. 


---

## Next Steps

- **Immediate:** Route HFP call audio to headphones — hear a real phone call
- **Then:** Simulation keys (KEY1-KEY6) + state machine + LED patterns
- **Then:** Physical phone integration when it arrives (hook switch GPIO 4, rotary dial GPIO 16/17)
- **V2:** WiFi + Gemini Live voice assistant

---

*Written April 2026. Berlin.*
