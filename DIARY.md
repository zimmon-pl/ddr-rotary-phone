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

### 2026-04-14 (late evening) — First call audio heard!

Implemented HFP Voice over HCI audio path. Made an outgoing call from the Pixel while paired to FestStefan — **voice came through the headphones**. Audio was quiet but audible. mSBC (Wide Band Speech) negotiated automatically at 16kHz.

**What we built:**
- Added `CONFIG_BT_HFP_AUDIO_DATA_PATH_HCI` and `CONFIG_BT_HFP_WBS_ENABLE` to sdkconfig
- Legacy data callback (`esp_hf_client_register_data_callback`) receives decoded PCM from BT stack
- Ring buffer between BT callback and I2S playback task (non-blocking, no BT task stalls)
- I2S reconfigured to 8kHz (CVSD) or 16kHz (mSBC) when call connects, back to 44100Hz on disconnect
- Mono→stereo conversion in playback task (ES8388 expects stereo I2S)
- Outgoing audio sends silence for now (microphone not connected yet)

**What the logs showed:**
- `Audio: connected_msbc` — mSBC codec selected (16kHz, better quality than CVSD 8kHz)
- Call audio started/stopped cleanly, I2S reconfigured back to 44100Hz after hang up
- Volume events flowing from phone (speaker volume 5→15)
- No crashes, no audio glitches in the short test

**Issue:** Audio too quiet — need to increase ES8388 output volume registers (`LOUT1VOL`/`ROUT1VOL`) or DAC digital volume.

**Binary size:** 702KB (33% of flash partition still free).

---

### 2026-04-17 — Full call loop working, mic path deferred

The big one. Today the phone actually behaves like a phone: it rings when someone calls, I lift the handset to answer, I hear the caller through the earpiece, and replacing the handset hangs up. Every piece of that sentence took a separate fix, but the end result is that FestStefan works end-to-end on the listening side.

**Fixed the silent earpiece.** Yesterday's call audio was technically working but inaudible — only the boot beep came through. Traced it to the volume: `audio_set_volume(60)` was writing -38 dB of digital attenuation on top of another -13 dB of analog attenuation on LOUT1VOL, for ~-51 dB total. The boot tone survives because it's played near full scale; voice samples from HFP are much quieter and got buried. Changed to `audio_set_volume(100)` (0 dB digital, full headroom) and eased LOUT1VOL from `0x15` to `0x1A`. Voice now comes through clearly.

**Gave up on the microphone — for now.** Spent a while trying to wire the electret capsule I bought (Sourcing Map 6mm) through the LINEIN jack with a salvaged airline-earphones plug. Got the cable identified (blue = Tip / Left / LINPUT1, bare copper = Sleeve / GND, red ring ignored), wired it up, but the other party heard nothing. Root cause: the LINEIN jack is AC-coupled through DC-blocking capacitors, which means the ES8388's MICBIAS can't reach the electret element — and an electret needs a DC bias to work at all. Options discussed:

1. MAX4466 / MAX9814 preamp module with its own bias and amplifier — cleanest path, needs 3.3V + GND + OUT (three wires from the base, one of which has to run into the handset if the preamp lives outside).
2. Soldering directly to the MIC1/MIC2 SMD pads on the A1S board — risky, those pads are tiny, a slip destroys the board.
3. Switching to a different ESP32 variant — ruled out: HFP requires Bluetooth Classic, and only the original ESP32 supports it. S3/C3 are BLE-only.

Decision: **punt the mic.** Order a MAX4466 (or similar) breakout, mount it inside the phone base near the ESP32, run one bias/GND pair down the handset cable to the capsule, one signal wire back. For this session, leave the mic unwired and focus on everything else that doesn't need it. That "everything else" turned out to be a lot.

**Hook switch integrated.** Wired St ↔ a on the RFT PCB to a GPIO and a GND line. The plan was GPIO 4, but I couldn't find IO4 on any accessible header on the A1S — switched to GPIO 23, which is on the bottom-left pin header next to GND. Software-side:

- `hook_switch.c` uses a GPIO interrupt + FreeRTOS queue + debounce task (50 ms). Interrupt service routine is tiny (just enqueues a dummy event); the task does the sleep + re-read of the pin level, which avoids the `esp_timer` ISR crash pattern from earlier in the project.
- Polarity came out inverted from what I'd coded: on this wiring, the GPIO reads LOW when the handset is lifted (St↔a closed) and HIGH when it's down (pulled up, contacts open). Flipped the level→state mapping in code rather than rewiring. Updated the comment in `hook_switch.c` to say so explicitly, so future me doesn't "fix" it back.
- Public API: `hook_switch_init()`, `hook_switch_on_change(cb)`, `hook_switch_get_state()`. `main.c` registers a callback and routes state changes into the Bluetooth and audio layers.

**Dial tone.** European dial tone is 350 Hz + 440 Hz continuous sine mix. Wrote a FreeRTOS task that generates the mixed sine in a small buffer and streams it to I2S TX in a loop while a `dial_tone_active` flag is true. `audio_start_dial_tone()` unmutes the DAC and spawns the task; `audio_stop_dial_tone()` clears the flag, waits 150 ms for the task to drain the buffer, then mutes the DAC. Both idempotent — safe to call twice.

**Call control wired to the hook.** In `main.c`, the `on_hook_change` callback now does the obvious thing:

- **Lift + ringing** → `bluetooth_answer_call()` (HFP AT+ATA under the hood).
- **Lift + idle** → `audio_start_dial_tone()`.
- **Replace** → `audio_stop_dial_tone()` (harmless if not playing), then `bluetooth_hangup_call()` if a call is in progress or ringing.

To make that work, `bluetooth.c` now mirrors HFP's CIND indicators into two local flags (`is_ringing`, `is_in_call`) and exposes them via `bluetooth_is_ringing()` / `bluetooth_is_in_call()`. Call control helpers wrap `esp_hf_client_answer_call()` and `esp_hf_client_reject_call()`.

**Tested the full loop.** Flashed, paired, ran it live with a real incoming call:

- Boot → `Initial state: ON CRADLE`, correct.
- Idle lift → `Handset LIFTED`, dial tone starts, DAC unmutes, audible through the handset.
- Idle replace → `Handset REPLACED`, dial tone task ends, DAC mutes.
- Incoming call while handset is down → `*** RING! Incoming call ***`, caller ID logged (phone number of the person who called — confirmed it was the right number).
- Lift during ring → `Answering call`, `Audio: connected_msbc`, call audio task + mic capture task start. Heard the other party clearly through the earpiece.
- Replace during call → `Hanging up`, audio cleanly torn down, I2S reconfigured back to 44.1 kHz, state returns to idle.
- Ran several more lift/replace cycles afterwards — dial tone behaves correctly, nothing sticks.

One aside from the log: during the very first ring cycle the remote party hung up before I lifted (reason code `0x13` = remote user ended). That's not a bug — it's just how the logs read when someone stops calling.

**What's still missing / known issues:**

- Mic is silent (as planned — waiting on MAX4466).
- No visual feedback yet — LED ring not wired.
- No rotary dial — can answer incoming calls but can't make outgoing ones from the phone.
- Reason 0x35 "bta_dm_pm_btm_status hci_status=35" appears occasionally in the log during sniff-mode transitions — harmless, Bluetooth stack recovering from a denied low-power request.

**Shift in scope.** The original plan had "use onboard MIC1/MIC2 as interim" as a fallback before the preamp arrives. Decided against it — those pads are SMD and require microsoldering. Better to wait for the MAX4466 and do it cleanly. The phone can still demonstrate one-way audio + hook control in the meantime, which is enough to show to people.

---

### 2026-04-16 — Stefan arrives! Phone opened, PCB mapped

Picked up the DDR RFT rotary phone from Moabit. Opened it, photographed and identified all terminals on the original RFT Serie 811 PCB. Created `WIRING.md` for the repo.

**PCB terminal map:**
- **NS1/NS2/NS3** (brown/white/green) = rotary dial contacts
- **St** = hook switch rail — terminals currently empty, need to solder new wires
- **Mi** (teal) = microphone terminal
- **Fe1/Fe2** (pink + white) = earpiece/speaker
- **WK, B2, ET1/ET2/ET3, b1–b4** = bells and telephone line — leave untouched
- **Black button (Erdtaste)** = shorts ET1↔ET3, office PBX function — unused

**Hook switch — how it works:**
Three-position switch on the St rail:
- Handset on hook → St ↔ ET2/ET3 (bell circuit)
- Handset lifted → St ↔ a + b (telephone line pair)
- Black button pressed → St ↔ ET1 (earth/PBX)

For ESP32: two wires from St and a/b, read as digital input on GPIO 4.

**Rotary dial contacts:**
- **NSI** (NS1/NS2) = pulse contact, normally closed, opens during return stroke
- **NSA** (NS3) = off-normal contact, normally open, closes when dial is moved from rest

Wire colors: brown, white, green — exact mapping to NSI/NSA to be confirmed when connecting to GPIO 16/17.

**Handset disassembly:**
- Original speaker: **HS-77 150Ω** — too high impedance for ESP32 codec
- Original microphone: **FEP** carbon capsule — decision deferred, using onboard mic for now
- Replaced speaker with new smaller 8Ω unit
- Handset cable: 4 wires — yellow = shared GND, green/yellow = speaker, rest = microphone

**First sound through the handset! ✅**
- New speaker wired directly to LOUT+/- on ESP32 Audio Kit V2.2
- Tone test plays through the handset speaker
- Problem: too loud and distorting — ES8388 output overdriving the small 8Ω speaker, need to reduce LOUT1VOL/ROUT1VOL registers

---

### 2026-04-21 — First solder joints, bias tee built, mic diagnostic marathon

Learned to solder today — first time ever touching an iron. The 80 € budget set from Amazon turned out to be fine for this. Did maybe a dozen joints over the evening. Some good, some ugly, one bad enough to become the plot twist of the day.

**Built the electret mic bias tee inside the phone base:**
- 2.2 kΩ resistor from the shared mic node to ESP32 3.3 V.
- 10 µF electrolytic from the shared mic node to LINEIN, with + on the mic-node side and − on the LINEIN side (AC-couples the audio, blocks DC bias from reaching the codec).
- Red wire from handset (mic +) joins the shared node.
- Yellow wire from handset (shared GND) runs straight to an ESP32 GND pin via Dupont.
- A mono 3.5 mm plug terminates the cap's output wire (tip = signal, sleeve = GND through the jack's internal contact).

**Handset cable colours finalised (updated [WIRING.md](WIRING.md) yesterday already):**
- Green = speaker +
- Yellow = shared GND (speaker return + mic return)
- Red = microphone +

**Speaker status:** Already installed inside the handset since 2026-04-16, wired straight to LOUT+/− on the A1S. That's been working end-to-end through HFP calls for days — the far party's voice comes through the earpiece. So **speaker side: done**. Only the mic path is the bottleneck.

**The diagnostic marathon.** Did a full BT call test after building the bias tee. Mic gain already lowered from `0xBB` (33 dB) to `0x88` (24 dB) in [main/main.c:52](main/main.c#L52) to avoid clipping the electret. Outgoing call worked, I could hear the far end clearly — but they heard absolutely nothing from me.

Multimeter hour began. Methodical walk through the circuit:

1. Measured DC voltage at the capsule + pin with black probe on ESP32 GND → **read 3.3 V**. That means no current was flowing, so either the capsule was wired backwards, dead, or the GND side was floating.
2. Swap test at the bias tee wasn't practical (shared GND with the working speaker), so instead: **continuity test from ESP32 GND to the capsule − pin inside the handset** → no beep. Found it: the mic's GND solder pad was cold — even though the speaker shared the yellow wire and worked fine, the mic's − pin was floating.
3. Re-flowed the joint. New measurement: **2.7 V at the + pin**. Not ideal (healthy electret wants ~1.5–2 V), but the capsule is now alive and drawing current.
4. Cap sanity check: continuity across the cap → short beep then silence (cap charging through the meter, then blocking DC — healthy). Multimeter's capacitance mode confirmed 10 µF. Cap is fine.
5. Continuity from cap output to the 3.5 mm plug tip → clean beep. Signal is routed to the right plug contact.

But the far end still couldn't hear me. One more measurement I *thought* was alarming — ~2.7 V on the LINEIN side of the cap — turned out to be a false positive from my own bad test advice: I had the plug *unplugged* while probing, which means the LINEIN wire was floating, and a floating wire near a 2.7 V source just picks up phantom voltage through the multimeter's 10 MΩ input impedance. Noted for next time: **measure the AC-coupled side of a cap with the load connected, not floating.**

**Bonus discovery:** KEY4 on the A1S board (GPIO 23) works as a hook-switch simulator. Pressing it during a call triggered `Handset LIFTED` / `REPLACED` events in the firmware and cleanly hung up. Useful for testing without the physical hook switch wired in.

**What I'm leaving the day with:**
- Bias tee built and physically sound.
- Capsule is alive and biased, but bias is 2.7 V instead of the ideal 1.5–2 V — probably the re-flowed GND joint still has a bit of resistance. A cleaner joint should pull it into range.
- Mic still silent on the far end. Could be low signal level (2.7 V bias → thinner capsule output), insufficient PGA gain, or a cap-related DC leak I haven't ruled out.
- One measurement remaining tomorrow: re-measure DC on the LINEIN side of the cap **with the plug inserted into the LINEIN jack** — that gives a real reference instead of the floating-wire artifact from today.

**Lesson learned about multimeter use:** continuity mode (power off, beep for yes/no) is the right tool for "is wire A connected to point B?" — faster and less ambiguous than chasing DC voltages. I used DC voltage for a test that should have been a continuity test and it cost us an hour. Won't make that mistake next time.

**Code change today:** lowered `audio_set_mic_gain()` argument in [main/main.c:52](main/main.c#L52) from `0xBB` (≈33 dB) to `0x88` (≈24 dB) because electret + bias tee produces more output than the old carbon-mic plan. May need to walk it back up if the capsule really is producing a thin signal at 2.7 V bias.

---

### 2026-04-24 — The A1S mic bug, discovered the hard way

Spent an entire session chasing the silent microphone through the ES8388 registers, and it ended with a hardware verdict: the A1S board is physically wired against us. The mic is fine, the bias tee is fine, the firmware is fine — the board itself shorts the LINEIN signal to ground before it ever reaches the ADC. Long day, but the answer is finally unambiguous.

**Warm-up: confirmed the wiring I deferred last time.**
- Hook switch pair on the RFT PCB: `St ↔ Et2` (continuity test with multimeter, handset on cradle).
- Rotary dial: three wires from the dial — green, brown, white. Continuity testing: `brown ↔ green` is the pulse pair (NSI), `green ↔ white` is the off-normal pair (NSA). **Green is the common.**
- Bias tee still reads 3.3 V on the bias side, 2.7 V on the LINEIN side. Stable. Healthy for an electret.

**Built a mic test mode so I didn't have to do a real Bluetooth call every time I wanted to see if the mic worked.** Added `audio_start_mic_test()` / `audio_stop_mic_test()` in [main/audio.c](main/audio.c): reads raw ADC samples off I2S RX, computes peak + RMS over each ~100 ms window, logs the numbers. Wired it to the on-board REC button (GPIO 36) in [main/main.c](main/main.c) — one press to start, another to stop. This turned every experiment into a 15-second loop: change a register, flash, press REC, watch the numbers, repeat.

**What the REC button showed me across four register tweaks.** Every run looked the same shape: ~5 seconds of silence, tap the mic, speak, blow, compare. Tracking idle baseline only, because *nothing I did made the numbers respond to my voice or a tap*:

| Change | Idle RMS | Reaction to speech |
|---|---|---|
| Stock config (LINPUT1, MICBIAS on, HPF off) | ~1470 | none |
| `ADCCONTROL2 = 0x55` → LINPUT2/RINPUT2 (LINEIN jack) | ~250 | none |
| `ADCCONTROL7 = 0x0A` → HPF enabled (it wasn't, the old value `0x20` actually *disabled* it) | ~320 | none |
| `ADCCONTROL1 = 0xFF` → PGA to max | ~317 | none (PGA clips at 0x88, no real gain above that) |
| `ADCPOWER = 0x09` → MICBIAS off (matches esp-adf reference) | ~170 | none |

Each tweak moved the idle floor but the signal itself never appeared. By the last row I had a clean, quiet baseline with no real input. That's when I gave in and searched.

**The smoking gun: Phil Schatzmann's 2021 write-up on the AI Thinker AudioKit input bug.** On the A1S V2.2, the PCB *physically bridges* the ES8388 pins — the onboard MEMS mics (MIC1, MIC2) are tied via coupling caps C18 and C20 to the same LIN2/RIN2 pins used by the LINEIN jack. An external mic plugged into LINEIN doesn't sit *alongside* the onboard mics — it sits in parallel with them. The onboard mic capsules load my external signal down to nothing. No ES8388 register can un-route a copper trace. The fix is hardware: move C18 → C17 and C20 → C19, or desolder the onboard mic capsules outright.

**Sanity check: the mic itself is not the problem.** Multimeter in AC millivolt mode, probes on the bias-tee output and GND. Silent → flat. Blow / tap / speak → the needle jumps. The external electret is alive, the bias tee is delivering real signal, the path is clean up to the edge of the A1S jack. Every other rotary-phone project I checked (Jouke's bakelite phone, derkorte's W48, weeBell) uses discrete audio chips — PCM5102 for DAC, WM8782 or INMP441 for ADC — specifically to avoid this class of codec-integration pain. Nobody sane uses the A1S codec for mic input on this kind of project.

**What I'm leaving the day with:**
- Mic debug loop (REC button → peak/RMS log) is a reusable tool — it'll stay in the firmware.
- All the register changes we made are *correct* configuration for external-mic-on-LINEIN usage on this codec. They just can't overcome the board-level short.
- The board is not lying to me, the mic is not lying to me, I was not losing my mind.

**Path forward.** Three choices, in order of effort:
1. **Desolder the two onboard MEMS mic capsules (MIC1, MIC2).** Regular iron, 10 minutes. Simplest way to unblock.
2. **Move the SMD caps C18 → C17 and C20 → C19.** Cleanest long-term fix, but 0402 SMD rework — needs fine-pitch soldering skills.
3. **Swap the mic path to a discrete ADC chip** (WM8782 or INMP441 breakout). Follows the well-worn path of every other rotary-phone project. Significant firmware changes. A1S stays on DAC duty, which is fine.

Leaning toward option 1 as the next experiment. Everything non-mic (speaker, BT pairing, incoming call audio, hook switch, dial tone) is still solid — we didn't break anything chasing this.

**Commits for this session:** REC-button mic test mode, codec re-routed to LINPUT2/RINPUT2, HPF enabled, MICBIAS off, PGA at max. [audio.h](main/audio.h) gets three new public functions. [main.c](main/main.c) gets a small polling task for GPIO 36.

---

## Next Steps

**Mic (next session):**
1. Desolder the two onboard MIC capsules on the A1S board (option 1 above). Iron, tweezers, heatgun if needed. Then re-run the REC-button mic test — expect silence to stay near 0 RMS and taps/speech to spike peaks into the thousands.
2. If option 1 doesn't get us there: plan the discrete-ADC path. Order a WM8782 or INMP441 breakout, design the I2S routing, write a minimal codec-agnostic mic driver.

**After the mic works:**
- Rotary dial — wire brown → GPIO 16, white → GPIO 17, green → GND per today's confirmed pairings, and call `rotary_dial_init()` from `main.c`. Module is already written and waiting.
- Wire the permanent 3.5 mm jack for the speaker path (replace the temporary direct-solder LOUT connection).
- LED ring (WS2812B) with state feedback — pulsing red for ringing, solid green for active call, breathing blue for idle/paired.
- Formal state machine (IDLE / RINGING / DIALING / IN_CALL) to replace the ad-hoc flag-checking in `on_hook_change`.

**V2:** WiFi + Gemini Live voice assistant.

---

*Written April 2026. Berlin.*
