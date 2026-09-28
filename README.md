<div align="center">

# 🎯 Stick Catcher

**A reaction game with 1 to 16 falling sticks, powered by an ESP8266, a PCA9685 servo driver and a phone-friendly web panel.**

![Platform](https://img.shields.io/badge/platform-ESP8266-blue?logo=espressif&logoColor=white)
![Framework](https://img.shields.io/badge/framework-Arduino-00979D?logo=arduino&logoColor=white)
![Build](https://img.shields.io/badge/build-PlatformIO-orange?logo=platformio&logoColor=white)
![License](https://img.shields.io/badge/license-MIT-green)
![Version](https://img.shields.io/badge/version-2.0.0--dev-purple)

</div>

Sticks hang from servo-driven hooks: **6 by default, and anywhere from 1 to 16** (one per PCA9685 channel). Press **START**, wait through the countdown, and the hooks let go of the sticks **one at a time, in a random order, at random intervals**. Every stick drops exactly once per round. Your job: catch them before they hit the floor.

The ESP8266 also creates its own Wi-Fi network, so you can start the game, stop it, pick how many sticks you play with, tune the timing and calibrate every servo from your phone. No router or internet needed.

> **Versions.** This is **v2** (in development on the [`v2`](https://github.com/Am4l-babu/stick-catching-game/tree/v2) branch): the number of sticks is selectable. The working six-stick version is tagged [**`v1.0.0`**](https://github.com/Am4l-babu/stick-catching-game/tree/v1.0.0). See the [changelog](CHANGELOG.md).

<p align="center">
  <a href="https://htmlpreview.github.io/?https://github.com/Am4l-babu/stick-catching-game/blob/v2/docs/simulator.html">
    <img src="docs/simulator-preview.png" alt="Stick Catcher browser simulator: servo hooks dropping glowing sticks, a phone-style control panel and a live serial monitor" width="900">
  </a>
</p>

<p align="center">
  <a href="https://htmlpreview.github.io/?https://github.com/Am4l-babu/stick-catching-game/blob/v2/docs/simulator.html"><b>▶ Play the simulator in your browser</b></a> · no hardware needed
</p>

---

## Table of contents

- [Try it in your browser](#-try-it-in-your-browser)
- [How to play](#-how-to-play)
- [Features](#-features)
- [Hardware](#-hardware)
- [Wiring](#-wiring)
- [Get the code onto the board](#-get-the-code-onto-the-board)
  - [Option A: VS Code + PlatformIO (recommended)](#option-a--vs-code--platformio-recommended)
  - [Option B: Arduino IDE](#option-b--arduino-ide)
  - [Option C: PlatformIO CLI](#option-c--platformio-cli)
- [Web control panel](#-web-control-panel)
- [Choosing the number of sticks](#-choosing-the-number-of-sticks)
- [Settings and calibration](#-settings-and-calibration)
- [Servo test sketch](#-servo-test-sketch)
- [How it works](#-how-it-works)
- [HTTP API](#-http-api)
- [Project structure](#-project-structure)
- [Troubleshooting](#-troubleshooting)
- [License](#-license)

---

## 🕹 Try it in your browser

Want to see the game before you build it? Open the **[interactive simulator](https://htmlpreview.github.io/?https://github.com/Am4l-babu/stick-catching-game/blob/v2/docs/simulator.html)**. It is a single self-contained page ([`docs/simulator.html`](docs/simulator.html)) that runs the same state machine as the firmware, so what you see matches what the hardware does.

| | |
| --- | --- |
| 🎯 **Play it** | Click a falling stick, tap a lane, or press its key to catch it: **1** to **9** and **0** for sticks 1 to 10, then **Q W E R T Y** for sticks 11 to 16. Press **Space** for the START button and **Esc** to stop. |
| 🔢 **Any number of sticks** | Drag **Number of sticks** in the phone's Sticks card (1 to 16) between rounds. The hooks, keys, scoreboard and test buttons all follow. |
| 🦾 **Live servos** | Animated hooks driven by the real HOLD/RELEASE angles. Drag the sliders and watch the horns move. |
| 📱 **The real web panel** | The phone on the right mirrors the control panel served at `192.168.4.1`, with the same sliders, buttons and status text. |
| 🖥 **Serial monitor** | Prints the same messages as the firmware (`Releasing STICK 3`, `Next stick after 1240 ms`, …). |
| 📊 **Scoreboard** | Reaction time per stick, average, fastest, letter grade and a saved personal best for each stick count. |
| 🤖 **Bot mode** | Turn on the auto-play bot and watch a round play itself. |
| 🧪 **Learn by breaking it** | Set RELEASE within 20° of HOLD and the hooks **JAM**, just like a badly calibrated build. Shorten the drop height for a harder game. |

Prefer to run it locally? Open `docs/simulator.html` in any browser. Add `?sticks=10` to the address to start with 10 sticks, or `?demo=1` to let the bot play a round automatically (they combine: `?demo=1&sticks=16`). You can also enable [GitHub Pages](https://docs.github.com/pages) on the `docs/` folder to host it at your own URL.

---

## 🎮 How to play

1. **Load the sticks.** Reset the game (or power it on). Every hook moves to the *hold* position. Hang one stick from each hook.
2. **Get ready.** Stand (or sit) below the sticks with your hands ready. One or more players can take part.
3. **Start.** Press the physical **START** button, or tap **▶ START GAME** in the web panel.
4. **Countdown.** Nothing happens for **5 seconds** by default. Use the time to get into position.
5. **Catch!** A random hook releases its stick. After a random pause of **0.5 to 2 s** the next one drops, and so on until every stick has fallen. You never know which stick is next or when.
6. **Round over.** Half a second after the last stick, every hook resets to *hold* and the game goes back to waiting. Reload the sticks and press START again.

**Scoring ideas** (the firmware does not keep score, so pick your own rules):

| Mode | Rule |
| --- | --- |
| Solo | Count how many sticks you catch. Beat your personal best. |
| Head-to-head | Two players, one hand each side. Whoever catches more wins the round. |
| Hard mode | Shrink the delays in the web panel (for example 100 to 600 ms) for a faster round, or add more sticks. |
| Marathon | Play 5 rounds and add up the catches. |

Press **■ STOP / RESET** at any time to abort a round and return every hook to *hold*.

---

## ✨ Features

- 🔢 **1 to 16 sticks.** Build as many hooks as you like, up to the 16 channels of one PCA9685, and pick the count in the web panel.
- 🎲 **Fair random order.** A Fisher-Yates shuffle guarantees each stick drops exactly once per round.
- ⏱ **Random timing.** Configurable start countdown and a min/max random gap between drops.
- 📶 **Built-in Wi-Fi access point.** No router needed. Connect to `STICK-CATCHER` and open a page.
- 📱 **Mobile web panel.** Start/stop, stick count, timing sliders, servo angle sliders and a test button for every hook.
- 🔘 **Physical START button.** Debounced, works alongside the web panel.
- 🧪 **Servo test mode.** Fire any single hook from the web panel to check your mechanics.
- 🔧 **Standalone servo test sketch.** Check wiring and find your servo limits from the serial monitor before running the game.
- 🖥 **Serial log.** Everything the game does is printed at 115200 baud.
- 🕹 **Browser simulator.** Play and explore the game without any hardware.
- 🧩 **Non-blocking state machine.** Web requests keep being served while a round is running.

---

## 🧰 Hardware

| Qty | Part | Notes |
| --- | --- | --- |
| 1 | ESP8266 dev board | NodeMCU v2 (ESP-12E) is the default. A Wemos D1 mini also works. |
| 1 | PCA9685 16-channel PWM/servo driver | I²C address `0x40` (default). |
| 1 to 16 | SG90 micro servos | One per stick (6 by default). Any 50 Hz hobby servo works. |
| 1 | Momentary push button | Wired to GND, uses the internal pull-up. |
| 1 | **5 V power supply** | Powers the servos only. 2 A for up to 6 servos, 3 A for up to 10, 5 A for up to 16. See the warning below. |
| n | Jumper wires, breadboard, hooks and sticks | Build the release mechanism to suit your sticks. |

> ⚠️ **Do not power the servos from the ESP8266's 3.3 V pin or its USB port.** Six SG90s can pull well over 1 A when they move together, and 16 can pull several amps. Feed them from a separate 5 V supply into the PCA9685's **V+** terminal, and **join the grounds** of the supply, the PCA9685 and the ESP8266.

---

## 🔌 Wiring

### Connections

| From | To | Purpose |
| --- | --- | --- |
| ESP8266 `D2` (GPIO4) | PCA9685 `SDA` | I²C data |
| ESP8266 `D1` (GPIO5) | PCA9685 `SCL` | I²C clock |
| ESP8266 `3V3` | PCA9685 `VCC` | Logic power |
| ESP8266 `GND` | PCA9685 `GND` | Common ground |
| External 5 V `+` | PCA9685 `V+` (terminal block) | Servo power |
| External 5 V `−` | PCA9685 `GND` | Common ground |
| ESP8266 `D5` (GPIO14) | Button pin 1 | START button |
| ESP8266 `GND` | Button pin 2 | START button |
| Servo 1 to 16 | PCA9685 channels `0` to `15` | Stick hooks. Servo *n* goes on channel *n − 1*. Use as many as you built, starting from channel `0`. |

### Diagram

```
                       ┌───────────────────┐
   USB power ─────────▶│  ESP8266 NodeMCU  │
                       │                   │
                       │  D1 (GPIO5) ──────┼────────▶ SCL ┐
                       │  D2 (GPIO4) ──────┼────────▶ SDA │
                       │  3V3 ─────────────┼────────▶ VCC │   ┌───────────────┐
                       │  GND ─────────────┼──┬─────▶ GND ├──▶│    PCA9685    │
                       │                   │  │           ┘   │               │
                       │  D5 (GPIO14) ──┐  │  │               │  CH0 ─ Servo 1│
                       └────────────────┼──┘  │               │  CH1 ─ Servo 2│
                                        │     │               │  CH2 ─ Servo 3│
                                  [START button]              │  CH3 ─ Servo 4│
                                        │     │               │  CH4 ─ Servo 5│
                                        └─────┤               │  CH5 ─ Servo 6│
                                              │               │  ⋮            │
                                              │               │CH15 ─ Servo 16│
                                              │               │               │
   External 5 V, 2–5 A ─── (+) ───────────────┼──────────────▶│  V+           │
                       └─── (−) ──────────────┴──────────────▶│  GND          │
                                                              └───────────────┘
```

Servo wire colours: **brown/black** = GND, **red** = V+, **orange/yellow** = signal. Plug them into the 3-pin headers with the signal wire on the `PWM` row.

**Pin summary** (all defined at the top of [`stick_catching_game.ino`](stick_catching_game/stick_catching_game.ino)):

| Function | NodeMCU pin | GPIO |
| --- | --- | --- |
| I²C SDA | D2 | 4 |
| I²C SCL | D1 | 5 |
| START button | D5 | 14 |

---

## 🚀 Get the code onto the board

Clone the repository first:

```bash
git clone https://github.com/Am4l-babu/stick-catching-game.git
cd stick-catching-game
```

Plug the ESP8266 into your computer with a **data-capable** micro-USB cable. You may need the [CP210x](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers) or [CH340](https://www.wch-ic.com/downloads/CH341SER_EXE.html) driver, depending on your board.

### Option A · VS Code + PlatformIO (recommended)

PlatformIO downloads the ESP8266 toolchain and the servo library for you, so there is nothing to install by hand.

1. Install [Visual Studio Code](https://code.visualstudio.com/).
2. Open the **Extensions** view (`Ctrl+Shift+X`), search for **PlatformIO IDE** and install it. Restart VS Code if it asks. The first launch takes a minute while PlatformIO sets itself up.
3. Choose **File ▸ Open Folder…** and open the **repository root** (the folder that contains `platformio.ini`).
4. Wait for PlatformIO to finish indexing. The first build also downloads the platform and libraries.
5. Use the buttons in the blue status bar at the bottom:

   | Button | Action |
   | --- | --- |
   | ✔ **Build** | Compile the code |
   | → **Upload** | Compile and flash the board |
   | 🔌 **Serial Monitor** | Open the serial log at 115200 baud |

   Or press `Ctrl+Alt+U` to upload and `Ctrl+Alt+S` to open the serial monitor.
6. If PlatformIO picks the wrong port, set it in [`platformio.ini`](platformio.ini):
   ```ini
   upload_port  = COM3        ; Windows, e.g. COM3
   monitor_port = COM3        ; Linux: /dev/ttyUSB0   macOS: /dev/cu.usbserial-*
   ```

**Using a Wemos D1 mini?** Switch environments in the status bar (the `Default (nodemcuv2)` picker) to `d1_mini`, or run `pio run -e d1_mini -t upload`.

### Option B · Arduino IDE

1. Install the [Arduino IDE](https://www.arduino.cc/en/software) (2.x recommended).
2. **Add the ESP8266 board package**
   - Open **File ▸ Preferences** and paste this into *Additional boards manager URLs*:
     ```
     http://arduino.esp8266.com/stable/package_esp8266com_index.json
     ```
   - Open **Tools ▸ Board ▸ Boards Manager**, search for **esp8266** and install *esp8266 by ESP8266 Community*.
3. **Install the servo library**
   - Open **Tools ▸ Manage Libraries…**, search for **Adafruit PWM Servo Driver Library** and install it. Accept the dependency prompt if it asks to install **Adafruit BusIO**.
4. **Open the sketch.** Choose **File ▸ Open…** and pick [`stick_catching_game/stick_catching_game.ino`](stick_catching_game/stick_catching_game.ino). Both `stick_catching_game.ino` and `web_page.h` open as tabs.
5. **Select the board and port**
   - **Tools ▸ Board ▸ esp8266 ▸ NodeMCU 1.0 (ESP-12E Module)** (or *LOLIN(WEMOS) D1 R2 & mini*).
   - **Tools ▸ Upload Speed ▸ 921600** (use 115200 if uploads fail).
   - **Tools ▸ Port ▸** the COM port of your board.
6. Click **Upload** (→). When it finishes, open **Tools ▸ Serial Monitor** and set **115200 baud** to watch the log.

### Option C · PlatformIO CLI

If you prefer the terminal (works without VS Code):

```bash
pip install platformio             # once

pio run                            # build
pio run -t upload                  # build + flash
pio device monitor                 # serial monitor, 115200 baud
pio run -e d1_mini -t upload       # flash a Wemos D1 mini instead
```

---

## 📱 Web control panel

1. Power the board. The status of the access point is printed on the serial monitor.
2. On your phone or laptop, join the Wi-Fi network:

   | | |
   | --- | --- |
   | **SSID** | `STICK-CATCHER` |
   | **Password** | `12345678` |

3. Open **<http://192.168.4.1>** in a browser. Your phone may warn that the network has no internet. Stay connected.

The panel has five cards:

| Card | What it does |
| --- | --- |
| **Status + Start/Stop** | Shows the live game state (`WAITING`, `STARTING IN 3s`, `RELEASING`, `WAITING - 2/10`, `RESETTING`). START begins a round; STOP/RESET aborts it and re-arms every hook. |
| **🎯 Sticks** | Slider for the number of sticks (1 to 16). **SAVE STICK COUNT** applies it and rebuilds the test buttons. Only works while the game is idle. |
| **⏱ Timing** | Sliders for start delay, minimum and maximum random gap, and servo release time. |
| **⚙ Servo Angles** | Sliders for the HOLD and RELEASE angles. |
| **🧪 Test Individual Servos** | One button per stick. Fires that hook (release, then return to hold). Only works while the game is idle. |

When the page opens it reads the board's current settings from `/config`, so the sliders always show what the board is really using.

> 💡 Change the Wi-Fi name and password by editing `AP_SSID` and `AP_PASSWORD` near the top of the `.ino`. The password must be at least 8 characters.

---

## 🔢 Choosing the number of sticks

1. **Build and wire the hooks.** Plug servo 1 into PCA9685 channel `0`, servo 2 into channel `1`, and so on with no gaps. Size the 5 V supply for the number of servos (see [Hardware](#-hardware)).
2. **Tell the game.** Either
   - open the web panel, set **Number of sticks** in the 🎯 Sticks card and press **SAVE STICK COUNT**, or
   - change `DEFAULT_STICKS` near the top of [`stick_catching_game.ino`](stick_catching_game/stick_catching_game.ino) and re-upload. Do this if you want the board to start with your count after every power-up.
3. **Check every hook.** Use the test buttons (one appears per stick) to fire each hook.

The count can only change between rounds. While a round is running the board answers `GAME RUNNING` and keeps the current count. New hooks move to HOLD as soon as the count is saved.

---

## 🎛 Settings and calibration

Everything below is a variable at the top of [`stick_catching_game.ino`](stick_catching_game/stick_catching_game.ino), and most of it can also be changed live from the web panel.

| Setting | Default | Web panel range | Meaning |
| --- | --- | --- | --- |
| `DEFAULT_STICKS` | `6` | 1 to 16 | Number of sticks at power-up (the web panel changes `stickCount`) |
| `holdAngle` | `0°` | 0 to 180° | Servo angle that *holds* a stick |
| `releaseAngle` | `90°` | 0 to 180° | Servo angle that *drops* a stick |
| `releaseTime` | `200 ms` | 50 to 1000 ms | How long the hook is given to move |
| `startDelay` | `5000 ms` | 1 to 15 s | Pause between START and the first drop |
| `minDelay` | `500 ms` | 100 ms to 5 s | Shortest random gap between drops |
| `maxDelay` | `2000 ms` | 200 ms to 10 s | Longest random gap between drops |
| `SERVO_MIN` | `150` | n/a (code only) | PCA9685 pulse count at 0° |
| `SERVO_MAX` | `600` | n/a (code only) | PCA9685 pulse count at 180° |

> ⚠️ Values saved from the web panel live in RAM only, so **they reset to the defaults above when the board restarts.** To make a setting permanent, change the default in the `.ino` and re-upload.

### Calibrating your servos

SG90 clones vary, so the defaults may not give a true 0° to 180° sweep.

1. Mount each hook and load a stick. Open the web panel.
2. In **Servo Angles**, set HOLD and RELEASE so the hook clearly holds the stick, then clearly lets go. Press **SAVE SERVO SETTINGS**.
3. Press the test button for each stick to test its hook. Adjust the angles or the hook mechanics until every stick releases reliably.
4. If a servo buzzes or hits its end stop, reduce `SERVO_MAX` or raise `SERVO_MIN` (for example 130 to 550) and re-upload.
5. Tune **Servo release time** so the hook has time to move but the round still feels snappy.

Not sure whether a problem is the wiring, the power or the game code? Flash the [servo test sketch](#-servo-test-sketch) first.

---

## 🔧 Servo test sketch

[`servo_test/servo_test.ino`](servo_test/servo_test.ino) is a small standalone sketch for **checking your wiring and calibrating the servos before you run the game**. It uses the same pins and the same PCA9685 settings as the game, and you control it by typing single-letter commands in the serial monitor.

Set `TOTAL_SERVOS` at the top of the sketch to the number of servos you built (1 to 16, default 6). On boot it scans the I²C bus (you should see the PCA9685 at `0x40`), moves every hook to HOLD and prints the command list.

| Command | Action |
| --- | --- |
| a servo number | Select that servo and fire it (release, wait, return to hold). Type `12` for servo 12. A single digit fires at once when no two-digit number could follow it; otherwise the sketch waits 0.7 s or until you press Enter. |
| `a` | Fire all servos in order |
| `w` | Slow 0° → 180° → 0° sweep of the selected servo |
| `+` / `-` | Nudge the selected servo by ±5° and print its angle and pulse count |
| `h` | Move all servos to HOLD |
| `r` | Move all servos to RELEASE |
| `i` | Scan the I²C bus |
| `?` | Show the help |

**Typical use**

1. Flash the sketch (see below) and open the serial monitor at **115200 baud**.
2. Type `i`. If nothing is found, fix SDA/SCL/power before anything else.
3. Type each servo number to make sure every hook moves. A dead servo points to a wiring or power problem on that channel.
4. Use `w` to see each servo's real travel, and `+`/`-` to find the exact HOLD and RELEASE angles for your mechanism. Copy them into `holdAngle` / `releaseAngle` in the game (or into the web panel).
5. If a servo buzzes or stalls near the ends of its travel, adjust `SERVO_MIN` / `SERVO_MAX` in both sketches.

**How to flash it**

| Toolchain | Steps |
| --- | --- |
| **VS Code + PlatformIO** | Click the PlatformIO icon ▸ **Project Tasks** ▸ **servo_test** ▸ **Upload**, then **Monitor**. Or run `pio run -e servo_test -t upload` and `pio device monitor`. |
| **Arduino IDE** | **File ▸ Open…** ▸ `servo_test/servo_test.ino`, select the same board and port as for the game, then **Upload**. Open the Serial Monitor at 115200 baud. |

When you are done, flash the game again (`nodemcuv2` environment in PlatformIO, or the `stick_catching_game` sketch in the Arduino IDE). The test sketch replaces the game on the board; it does not run alongside it.

---

## ⚙️ How it works

The game is a small non-blocking state machine that runs inside `loop()`:

```mermaid
stateDiagram-v2
    [*] --> WAITING
    WAITING --> COUNTDOWN: START button / web START<br/>(reset servos, shuffle order)
    COUNTDOWN --> RELEASE_STICK: startDelay elapsed
    RELEASE_STICK --> BETWEEN_STICKS: release next stick,<br/>pick random delay
    BETWEEN_STICKS --> RELEASE_STICK: delay elapsed
    RELEASE_STICK --> RESETTING: all sticks released
    RESETTING --> WAITING: all hooks back to HOLD
    COUNTDOWN --> WAITING: STOP
    BETWEEN_STICKS --> WAITING: STOP
```

- **Random order.** At START the array `[0, 1, …, stickCount − 1]` is shuffled with Fisher-Yates and released in that order, so each stick drops exactly once.
- **Servo control.** The ESP8266 talks to the PCA9685 over I²C (`Wire.begin(4, 5)`) at 50 Hz. Angles are mapped to pulse counts between `SERVO_MIN` and `SERVO_MAX`.
- **Hooks stay open.** A released servo remains at the release angle until the round ends, then all of them return to HOLD together.
- **Networking.** The board runs as a Wi-Fi access point (`WIFI_AP`) with an `ESP8266WebServer` on port 80.
- **Random seed.** `randomSeed(micros())` at boot, so each power-up gives a different sequence.

---

## 🌐 HTTP API

The web panel is a thin wrapper around a few `GET` endpoints, so you can script the game too:

| Endpoint | Parameters | Description |
| --- | --- | --- |
| `/` | none | The control panel (HTML) |
| `/start` | none | Start a round (ignored if one is running) |
| `/stop` | none | Abort and reset all servos |
| `/status` | none | Plain-text state, e.g. `WAITING - 3/10` |
| `/config` | none | Current settings as JSON: `sticks`, `maxSticks`, `start`, `min`, `max`, `release`, `hold`, `releaseAngle` |
| `/sticks` | `count` (1 to 16) | Set the number of sticks. Idle only: returns `409 GAME RUNNING` during a round |
| `/settings` | `start`, `min`, `max`, `release` (ms) | Update timing |
| `/servo` | `hold`, `release` (degrees) | Update servo angles |
| `/test` | `servo` (0 to sticks − 1) | Test one hook (idle only) |

```bash
curl http://192.168.4.1/start
curl "http://192.168.4.1/settings?start=3000&min=300&max=1200&release=200"
curl http://192.168.4.1/status
curl "http://192.168.4.1/sticks?count=10"
curl http://192.168.4.1/config
```

---

## 📁 Project structure

```
stick-catching-game/
├── stick_catching_game/            ← the sketch (Arduino IDE opens this folder)
│   ├── stick_catching_game.ino     ← game logic, servo control, web handlers
│   └── web_page.h                  ← the HTML/CSS/JS control panel
├── servo_test/                     ← standalone wiring/calibration sketch
│   └── servo_test.ino
├── pio/
│   └── servo_test_entry.cpp        ← lets PlatformIO build servo_test
├── docs/
│   ├── simulator.html              ← interactive browser simulator
│   └── simulator-preview.png       ← screenshot used in this README
├── platformio.ini                  ← PlatformIO build config (VS Code / CLI)
├── .vscode/
│   └── extensions.json             ← recommends the PlatformIO extension
├── .gitignore
├── CHANGELOG.md                    ← what changed between versions
├── LICENSE
└── README.md
```

The same sketch folders are used by both toolchains, so there is a single copy of the code. `platformio.ini` points PlatformIO's `src_dir` at `stick_catching_game/` for the game, and the `servo_test` environment builds `servo_test/servo_test.ino` through the small wrapper in `pio/`.

| PlatformIO environment | What it builds |
| --- | --- |
| `nodemcuv2` (default) | The game, for NodeMCU |
| `d1_mini` | The game, for Wemos D1 mini |
| `servo_test` | The servo test sketch (NodeMCU) |

---

## 🩺 Troubleshooting

| Symptom | Likely cause and fix |
| --- | --- |
| **Upload fails / "port not found"** | Try another USB cable (many are charge-only). Install the CP210x/CH340 driver. Hold **FLASH** while uploading starts if your board needs it. Lower the upload speed to 115200. |
| **`STICK-CATCHER` network doesn't appear** | Wait a few seconds after boot. Check the serial monitor for the IP address. Make sure the code uploaded. |
| **Page won't load** | Stay connected even if the phone says "no internet". Turn off mobile data and VPN. Use `http://` (not `https://`) and `192.168.4.1`. |
| **Servos don't move** | Flash the [servo test sketch](#-servo-test-sketch) and type `i` to check the PCA9685 is found. Check the external 5 V supply and that **all grounds are joined**. Verify SDA to D2 and SCL to D1. Confirm the PCA9685 address is `0x40`. |
| **Servos jitter, or the ESP8266 keeps resetting** | The power supply is too weak or the servos are drawing from the ESP. Use a dedicated 5 V supply sized for your servo count (2 A for 6, up to 5 A for 16) and add a 470 to 1000 µF capacitor across V+/GND on the PCA9685. |
| **Sticks don't release, or drop early** | Recalibrate: adjust HOLD/RELEASE angles and the hook geometry. See [Calibrating your servos](#calibrating-your-servos). |
| **Settings or stick count reset after reboot** | By design. Web-panel values live in RAM. Change the defaults (including `DEFAULT_STICKS`) in the `.ino`. |
| **A test button does nothing for one stick** | Check that servo is on the right channel (servo *n* on channel *n − 1*) and that the stick count includes it. |
| **Serial monitor shows garbage** | Set the baud rate to **115200**. |
| **Compile error: `Adafruit_PWMServoDriver.h` not found** | Arduino IDE: install *Adafruit PWM Servo Driver Library*. PlatformIO installs it automatically. |

---

## 📄 License

Released under the [MIT License](LICENSE). Have fun, and don't drop the sticks. 🎯
