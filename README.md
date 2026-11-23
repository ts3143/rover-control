# Rover Control

Drive our 4-motor rover from a Mac over USB with a **Logitech F310 gamepad** (or the keyboard), using a NodeMCU (ESP8266) and an L298N motor driver.

```
F310 --USB--> Mac (Chrome)  --USB/CP2102-->  NodeMCU ESP8266  -->  L298N  -->  2 left + 2 right motors
```

## Repo layout

| Path | What it is |
|---|---|
| `firmware/rover_esp8266/rover_esp8266.ino` | Arduino firmware for the NodeMCU |
| `web/rover-control.html` | Browser control panel: F310 gamepad + keyboard (Chrome/Edge, Web Serial + Gamepad API) |
| `tools/controller.py` | Optional terminal controller (`pip3 install pyserial`) |
| `docs/wiring-diagram.png` | Team wiring diagram |

## Hardware

- NodeMCU ESP8266 (ESP-12E / "ESP8266MOD", CP2102 USB chip)
- L298N motor driver: ENA/ENB jumpers **removed**
- 4 DC motors: left pair in parallel, right pair in parallel
- Motor power: bench supply or battery, 7–9 V, current limit ≥ 2 A
- HC-SR04 ultrasonic sensor (wired, not used in firmware yet)

### Wiring

| L298N | NodeMCU |
|---|---|
| ENA | D1 |
| IN1 | D5 |
| IN2 | D6 |
| IN3 | D7 |
| IN4 | D4 |
| ENB | D2 |
| GND | GND (common ground, required) |

| HC-SR04 | NodeMCU |
|---|---|
| TRIG | D0 |
| ECHO | D8 |

Motor outputs as built: **right pair on OUT1/OUT2, left pair on OUT3/OUT4** (left pair polarity reversed).
This is handled in firmware in `sides()`. If you rewire the motors, change only that function.

See `docs/wiring-diagram.png` for power (5 V, 3.3 V regulator, 12 V).

## Setup

### 1. Arduino IDE (one time)
1. **Settings → Additional boards manager URLs**:
   `http://arduino.esp8266.com/stable/package_esp8266com_index.json`
2. **Boards Manager** → install **esp8266 by ESP8266 Community**.

### 2. Upload firmware
1. Open `firmware/rover_esp8266/rover_esp8266.ino`.
2. Board: **NodeMCU 1.0 (ESP-12E Module)**. Port: `/dev/cu.usbserial-XXXX`.
   - Compile output should say *Maximum is 1044464 bytes*. If it says 1310720, an ESP32 board is selected.
3. Upload. Wait for *Hard resetting via RTS pin…*. If stuck on *Connecting…*, hold **FLASH**.

### 3. Check
Serial Monitor at **115200** → press **RST** → `ROVER READY`. Type `F` → `CMD F`.
Close the Serial Monitor before using the control panel (only one app can hold the port).

### 4. Drive
```bash
open -a "Google Chrome" web/rover-control.html
```
Click **Connect rover**, pick the `usbserial` port, then drive or click **Run motor test** (wheels off the ground first).

> Re-upload the firmware from this branch first — the gamepad uses the new `M` command (older firmware ignores it).

Want to see the UI without the rover or a gamepad? Open `web/rover-control.html?demo` (simulated F310 input; needs a local server, e.g. `python3 -m http.server -d web`).

## Gamepad (Logitech F310)

1. Set the switch on the back of the F310 to **D** (DirectInput). macOS doesn't support X (XInput) mode without extra drivers.
2. Plug it into the Mac and press any button — the header shows **Logitech F310** in green.
3. Make sure the **MODE** light is **off** (when it's on, the D-pad and left stick are swapped).
4. Press **A** to arm. The rover only obeys the gamepad while armed.

| F310 | Action |
|---|---|
| Left stick ↕ | Throttle (arcade) · left wheels (tank) |
| Right stick | Steer ↔ (arcade) · right wheels ↕ (tank) |
| D-pad | Drive / spin at the speed limit |
| RT (hold) | Turbo — full speed |
| LT (hold) | Precision — 40% of the speed limit |
| LB / RB | Speed limit − / + |
| A | Arm |
| **B** | **Emergency stop + disarm** |
| Y | Toggle arcade / tank mode |

Sticks are proportional (variable speed per side), with a 12% deadzone. Speed is scaled so the smallest stick movement is still ~PWM 60, below which the motors only hum.
If the rover turns the wrong way with the stick, flip **Invert steering** in the panel (or fix `sides()` in firmware).
Leaving the window, unplugging the gamepad, or pressing B disarms it. Keyboard input overrides the gamepad.

The four wheels are shown individually in the UI, but with one L298N the two wheels on each side are wired in parallel and always get the same command. Independent per-wheel control would need a second driver (e.g. another L298N) and four PWM channels.

## Keyboard

| Key | Action |
|---|---|
| W / ↑ | All motors forward |
| S / ↓ | All motors backward |
| A / ← | Left pair forward, right pair backward |
| D / → | Right pair forward, left pair backward |
| Space | Stop |
| 1–9 | Speed |

Hold to drive; release to stop. **Latch mode** keeps moving after release.
Failsafe: motors stop if no command arrives for 500 ms (cable unplugged, tab closed, window unfocused).

## Serial protocol

115200 baud, single ASCII characters:

| Char | Meaning |
|---|---|
| `F` `B` `L` `R` | Forward, back, left, right |
| `S` | Stop |
| `0`–`9` | Speed (0 = off, 9 = full PWM) |
| `M<left> <right>\n` | Proportional drive, each side −255..255 PWM, e.g. `M200 -120` (used by the gamepad) |

The sender must repeat the current command at least every 500 ms while moving.
Firmware replies: `ROVER READY`, `CMD <x>` (`CMD M` when analog driving starts), `FAILSAFE STOP`.

## Troubleshooting

| Symptom | Fix |
|---|---|
| Log shows `Ai-Thinker … ready` / `ERROR` | Factory AT firmware still on board; upload the firmware |
| *This chip is ESP8266, not ESP32* | Wrong board selected; pick NodeMCU 1.0 |
| Garbled text in Serial Monitor | Set baud to 115200 (garbage right after reset is normal boot output) |
| Motors hum but don't turn | Raise supply current limit / voltage, or use speed 7–9 |
| A pair won't run one direction | Check IN3/IN4 wiring to D7/D4 |
| One motor of a pair spins backwards | Swap that motor's two wires |
| Gamepad not detected / yellow warning | Switch on the back to **D**, unplug and replug, press a button with the page focused |
| Stick and D-pad swapped | Press the **MODE** button so its light is off |
| `requestPort` error in browser | Open the HTML directly in Chrome, not in a preview pane |
| Board won't boot with L298N connected | Unplug IN4 (D4), press RST, reconnect |

## Next up
- HC-SR04 obstacle stop (TRIG D0, ECHO D8)
- Wi-Fi control (ESP8266 web server) instead of USB cable
