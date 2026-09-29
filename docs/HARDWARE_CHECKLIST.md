# v3 hardware bring-up checklist

Everything in v3 has been tested against the simulator and against fake
hardware (`test/firmware/`), but never on a real board. Work through this
list once, in order, when you first flash v3. Each step says what should
happen and what to check in the serial monitor (115200 baud) if it doesn't.

Tick items off as you go; a failed item usually explains the next one, so
stop at the first failure and fix it before moving on.

## 0. Before you flash

- [ ] Build all environments once, so a bad build doesn't cost you a flash
  cycle: `pio run -e nodemcuv2 -e d1_mini -e servo_test`
- [ ] Six servos (or however many you're starting with) wired to PCA9685
  channels 0..N-1, board at I2C address `0x40`. See [README § Wiring](../README.md#-wiring).
- [ ] Servo power is a **separate 5 V supply**, not USB — 2 A is enough for
  6 servos. Grounds joined between the supply, the PCA9685 and the ESP8266.
- [ ] Run the [servo test sketch](../README.md#-servo-test-sketch) first if
  you haven't calibrated HOLD/RELEASE angles before. It's the fastest way
  to separate a wiring problem from a game-logic problem.

## 1. First boot

Flash `nodemcuv2` (or `d1_mini`), open the serial monitor.

- [ ] Banner prints (`STICK CATCHER`, `Firmware 3.0.0`)
- [ ] `No saved settings, using defaults.` — this exact line, only on the
  very first boot ever (or after `/defaults`)
- [ ] `Servo boards: 1 (up to 16 sticks)` — change to `2 (up to 32 sticks)`
  only if you actually wired a second PCA9685 at `0x41`
- [ ] `Sticks: 6 of 16`, `Mode: CLASSIC`
- [ ] `Floor sensors: none (no scoring)` unless you wired an MCP23017
- [ ] All 6 hooks move to HOLD within about a second (100 ms apart — you
  should be able to see them step, not snap together)
- [ ] `WiFi Access Point:` block prints an IP of `192.168.4.1`

**If the hooks don't move:** check `0x40` shows up on the I2C scan in the
servo test sketch before going further — this isn't a v3-specific bug, so
don't debug it here.

## 2. Wi-Fi and the panel

- [ ] `STICK-CATCHER` network appears within a few seconds
- [ ] Joining it **opens the control panel by itself** (captive portal) —
  this is new in v3; earlier versions needed you to type the URL. If it
  doesn't pop up, opening `http://192.168.4.1` manually must still work,
  and any other address (e.g. `http://google.com`) should also redirect
  there (this is the `handleNotFound` → 302 redirect).
- [ ] The **Board** card's `Firmware 3.0.0` / `Servo boards: 1` /
  `Floor sensors: none` line matches what the serial monitor printed

## 3. Settings survive a reboot

This is the headline feature of v3 — verify it actually persists.

- [ ] In the panel, change something obviously non-default in each card:
  e.g. sticks → 4, start delay → 3000 ms, hold angle → 10°, mode →
  Speed-up. Press each card's SAVE button and watch for the confirmation
  toast and the matching serial log line (`Settings saved to flash.`).
- [ ] **Power-cycle the board** (unplug/replug, not just reset) — this
  matters because EEPROM.commit() on ESP8266 is a flash write, and a
  cold boot is the real-world case that reset doesn't fully exercise.
- [ ] On reboot: `Settings loaded from flash.` and every value you changed
  is back exactly as you left it (check `/config` if you want the raw
  numbers)
- [ ] In the Board card, press **RESTORE DEFAULTS** → confirms →
  everything goes back to the `settings.h` defaults, and `/config`
  matches. Reboot once more and confirm the *defaults* also persisted
  (defaults are saved, not just applied)

**If settings don't persist:** check the serial monitor for a "checksum
mismatch" style silent fallback — `settingsLoad()` in `settings.h` returns
false and re-defaults if the magic number or checksum don't match, which
would print `No saved settings, using defaults.` on every boot even
though nothing is actually wrong with the flash hardware.

## 4. Each game mode, one real round

Play a full round in each mode with the sticks actually loaded, not just
watching the servos.

- [ ] **Classic** — one stick at a time, in a random (never-repeating)
  order, LED off while WAITING, blinking fast during the countdown, solid
  during the round
- [ ] **Speed-up** — play two rounds back to back without STOP; the gaps
  in round 2 should feel shorter than round 1 (or check the serial log's
  `Next stick after N ms` lines — N should be lower). Then press STOP and
  start a fresh round: gaps should be back to normal
- [ ] **Double drop** — two hooks release at (visibly) the same instant,
  repeatedly through the round
- [ ] **Marathon** (set rounds=2, pause=10s to keep the test short) —
  after round 1, hooks reset, LED blinks slowly, serial monitor prints
  `Reload the sticks!`, and round 2 starts by itself after the pause
  without you touching START

## 5. Per-hook trim

- [ ] In **Test & Fine-tune**, tap stick 1 → it fires once (release, hold)
- [ ] Move the Trim slider for stick 1 a few degrees either way, save —
  **only stick 1's hook** should move to its new HOLD position; the
  others must not twitch
- [ ] Confirm the trim survives a reboot alongside the other settings
  (step 3)

## 6. START button and STOP

- [ ] Physical button starts a round the same as the web START button
- [ ] A quick, deliberate press registers (this is the debounce — a
  20-30 ms bounce shouldn't double-trigger, but a normal press shouldn't
  get eaten either)
- [ ] STOP mid-round: every hook returns to HOLD, state goes back to
  WAITING, and a new round can be started immediately after

## 7. Wireless update

Do this **last**, once you trust the build, since a bad OTA image can
brick the running firmware until you fall back to USB.

- [ ] Build a `.bin` (`pio run -e nodemcuv2`, file is in
  `.pio/build/nodemcuv2/firmware.bin`)
- [ ] While still joined to `STICK-CATCHER`, open
  `http://192.168.4.1/update`, log in as `admin` / `stickcatcher` (or
  your own `OTA_PASSWORD`), upload the `.bin`
- [ ] Board restarts running the same firmware version, and your saved
  settings from step 3 are still there afterwards
- [ ] `pio run -e nodemcuv2_ota -t upload` also works from the CLI

## 8. Optional: second servo board / floor sensors

Only if you've actually built these.

- [ ] Second PCA9685 at `0x41` → serial monitor says
  `Servo boards: 2 (up to 32 sticks)`, stick count slider in the panel
  goes to 32, sticks 17+ move hooks on the second board
- [ ] Remove/disconnect it and reboot → falls back to 16 sticks and
  prints the `WARNING: N sticks saved but board 2 (0x41) not found.`
  line if you'd saved a count above 16
- [ ] MCP23017 floor sensors at `0x20` → `Floor sensors: 16 lanes
  (scoring on)`, and after a round the panel shows caught/missed counts
  that match what actually happened
- [ ] A sensor that's already blocked (stick resting on it) before the
  round starts should **not** count as an immediate miss — only a new
  landing during the round counts

---

Found a real bug? Please open an issue with the exact checklist step, what
you expected vs. what happened, and the relevant serial monitor output —
that's the fastest way to get it fixed.
