# Project Diary — DDR Rotary Phone Bluetooth Build

## The Spark

It started with a simple question: *can I make an old rotary phone work as a Bluetooth device so I can talk through WhatsApp?* What followed was a long, winding, genuinely fun conversation that went from a DIY electronics idea to a full project spec, a shopping list, a router audit, a Hikvision camera setup, and a smart home tour — before landing back on the phone.

---

## What I Bought

**The phone:**
- DDR RFT rotary phone, Typ 550-14012, VEB Fernmeldewerk Nordhausen, 1981 — found on Kleinanzeigen Berlin (Moabit), €20

**Electronics:**
- ESP32 Audio Kit A1S (Ai-Thinker, ES8388 codec) — ~€29, Amazon.de
- Shelly Plus 1PM WiFi relay (for the 12V lamp project) — ~€14, Amazon.de
- AstroAI AM33D digital multimeter — ~€16.58, Amazon.de
- KELLYSHUN KL-558 flux + desoldering braid set — ~€8.49, Amazon.de
- Lead-free solder 0.6mm, 20g — ~€5.79, Amazon.de
- Heat shrink tube set 328 pieces — ~€4.99, Amazon.de
- QWORK third hand with magnifying glass — ~€9.84, Amazon.de
- Velcro mounting tape 50x100mm — ~€5.99, Amazon.de
- Micro USB cable — already bought
- WS2812B LED ring — TBD (measure phone dial cavity first)
- 80W soldering iron set — ~€21.99, Amazon.de (budget option, sufficient for 6 solder joints)
- USB wall charger for ESP32 power — using existing charger from home

**Total hardware spend:** ~€160 including the phone

---

## What Tools We Used During This Chat

- **Gmail MCP** — dug up the original Hikvision camera order (Allegro, 2021), the LED bulb order (BA15S, 12V), and found the construction notes from Mateusz about CAT5/6 cable layout in Mnichowice
- **Web search** — found Amazon.de product links, checked Shelly/Sonoff prices, researched ESP32 Audio Kit variants, found Jouke's "Bakelite to the Future" project, looked up Vodafone DNS settings, checked HWD-7108MH-G2 firmware availability
- **nmap on Mac** — scanned the home network, identified 17 devices including the Shelly ESP32, two BSH appliances, Tuya bulbs, Google Nest speakers
- **Router audit** — reviewed all TP-Link Archer AX10 settings via screenshots: WiFi, DHCP, DNS, firewall, ALG, port forwarding, UPnP, IGMP, IPTV

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

**Camera system (Mnichowice):**
- Discovered via Gmail that 5x TVI cameras + 8ch HiWatch DVR (HWD-7108MH-G2) were bought in 2021
- Cameras connect via CAT5/6 + video baluns, not PoE
- 2 uninstalled cameras need: 2x video balun (~14 zł each) + 2x power connector (~2 zł each)
- DVR firmware is V3.5.37 (2018), too old for Remote Config — needs USB flash update to V4.30.101 when on site

**WiFi setup:**
- Vodafone Station in bridge mode → TP-Link Archer AX10 as router
- Full router audit: WPA2 on 2.4GHz, WPA3+WPA2 on 5GHz, channel 6, DHCP DNS updated to 1.1.1.1 + 8.8.8.8, UPnP reviewed, SIP ALG noted
- Smart home stays on 2.4GHz, phones/Mac on 5GHz — separate SSIDs by design

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
6. Clarified that TVI cameras in Mnichowice need baluns + power connectors, not PoE
7. Investigated HWD-7108MH-G2 DVR — confirmed firmware update path via USB pendrive
8. Audited and optimised home WiFi router (TP-Link Archer AX10) — DNS, channels, security
9. Identified BA15S 12V lamp via Gmail — chose Shelly Plus 1PM on 230V side as smart switch
10. Designed full MVP user journey and V2 scope
11. Decided on Gemini Live as V2 AI interface instead of hardcoded smart home shortcuts
12. Wrote complete project spec document ready for Cowork + Claude Code
13. ESP32 Audio Kit A1S ordered — arriving soon
14. VS Code + ESP-IDF already set up and ready

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

## Next Steps

- **Immediate:** Bluetooth HFP — pair as "FestStefan", route call audio to headphones
- **Then:** Simulation keys (KEY1-KEY6) + state machine + LED patterns
- **Then:** Physical phone integration when it arrives (hook switch GPIO 4, rotary dial GPIO 16/17)
- **V2:** WiFi + Gemini Live voice assistant

---

*Written April 2026. Berlin.*
