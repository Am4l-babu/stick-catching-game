# Changelog

## v2.0.0 (in development, `v2` branch)

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
