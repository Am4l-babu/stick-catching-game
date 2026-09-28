# Changelog

## v3.0.0

Game modes, saved settings, scoring and room for 32 sticks.

### Firmware
- **Settings saved to flash.** Stick count, timing, angles, trims, mode and buzzer setting survive a restart (`settings.h`, checksummed, only written when something changes). New `/defaults` endpoint and a **RESTORE DEFAULTS** button. The defaults moved to `settings.h`.
- **Per-hook trim** (−45° to +45°), added to that hook's HOLD and RELEASE angles. New `/trim` endpoint.
- **No blocking.** Servo resets, test fires, sounds and the START button debounce are all timed from `loop()` instead of `delay()`, so the web panel answers mid-round. The first drop waits until every hook is back at HOLD.
- **Game modes:** Classic, Speed-up (gaps ×0.85 per round), Double drop and Marathon (2–20 rounds with a 3–60 s reload pause). New `/mode` endpoint.
- **Buzzer** on D6 and **status LED** on D7. New `/sound` endpoint.
- **Floor sensors** (optional): MCP23017 expanders at `0x20`/`0x21` score each stick as caught or missed.
- **Second PCA9685** at `0x41` is detected at boot and allows up to 32 sticks.
- **Live `/state` JSON** (lane letters, round, speed, score) for the panel; `/config` also reports mode, trims, boards, sensors and firmware version.
- **Captive portal.** A DNS server sends every address to the panel, so phones open it on joining.
- **Wireless updates** through `/update` in the browser and ArduinoOTA (new `nodemcuv2_ota` PlatformIO environment).
- The panel page is stored in flash (`PROGMEM`) and streamed with `send_P()`: RAM use at boot went from 47% to 40%.

### Web control panel
- New **🎮 Game Mode**, **🧪 Test & Fine-tune** (trim) and **🔧 Board** cards.
- A dot per stick shows hanging, dropped, caught or missed; the status card shows mode, round, speed and score.
- Every phone with the panel open follows the same game and picks up changes made on another phone.
- Save buttons confirm with a message; changes that need an idle game say *Stop the game first*.

### Servo test sketch
- Up to 32 servos with a second PCA9685 at `0x41`. The I²C scan names the servo boards and sensor expanders.

### Simulator
- Mirrors v3: game modes, per-hook trim, up to 32 sticks (catch keys follow the keyboard rows), the non-blocking reset, marathon reload screen, LED indicator and the new phone cards.
- Settings are kept in the browser. New `?mode=` address option.
- Hosted on GitHub Pages.

### Project
- GitHub Actions builds every PlatformIO environment and runs a game logic test (`test/firmware/`) that plays the real firmware against fake hardware.

## v2.0.0

Choose how many sticks you play with, from 1 to 16.

### Firmware
- The number of sticks is now a setting (`stickCount`) instead of a fixed 6. It can be anything from 1 up to `MAX_STICKS` (16, one per PCA9685 channel). The power-up value is `DEFAULT_STICKS` (6).
- New endpoint `/sticks?count=N` sets the count between rounds. During a round it returns `409 GAME RUNNING`.
- New endpoint `/config` returns the current settings as JSON.
- Status text, serial log and servo test validation all use the current count (`WAITING - 3/10`, `ALL 10 STICKS HAVE FALLEN`).

### Web control panel
- New **🎯 Sticks** card with a slider and a **SAVE STICK COUNT** button.
- The test buttons are built from the stick count: 2 per row up to 6 sticks, then a compact 4-per-row grid.
- On load the panel reads `/config`, so every slider shows the board's real values instead of the defaults.
- Declares UTF-8, so `°` and the emoji render correctly on every browser.

### Servo test sketch
- Supports up to 16 servos (`TOTAL_SERVOS`). Type multi-digit servo numbers such as `12`.

### Simulator
- **Number of sticks** slider in the phone's Sticks card (locked while a round is running) and a `?sticks=N` URL option.
- The stage fits 1 to 16 hooks, with 16 stick colours and compact labels for narrow lanes.
- Catch keys: `1`–`9`, `0`, then `Q W E R T Y` for sticks 11 to 16.
- Scoreboard, grades and the saved personal best follow the stick count. The grade bands scale with the count, and a best score is kept for each count.

## v1.0.0

The working six-stick version: 6 servos on PCA9685 channels 0 to 5, Wi-Fi web control panel, physical START button, servo test sketch and browser simulator.
