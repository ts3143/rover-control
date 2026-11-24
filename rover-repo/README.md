# Rover Control

Drive our 4-motor rover from a Mac over USB, using a NodeMCU (ESP8266) and an L298N motor driver.

```
Mac (Chrome)  --USB/CP2102-->  NodeMCU ESP8266  -->  L298N  -->  2 left + 2 right motors
```

## Repo layout

| Path | What it is |
|---|---|
| `firmware/rover_wifi/` | **v2 firmware (recommended):** Wi-Fi joystick control from phone/laptop + game controller |
| `firmware/rover_esp8266/rover_esp8266.ino` | v1 firmware: USB-only control from a Mac |
| `web/rover-control.html` | Browser control panel (Chrome/Edge, Web Serial) |
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
Click **Connect**, pick the `usbserial` port, then drive or click **Run motor test** (wheels off the ground first).

## Controls

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

The sender must repeat the current command at least every 500 ms while moving.
Firmware replies: `ROVER READY`, `CMD <x>`, `FAILSAFE STOP`.

## Troubleshooting

| Symptom | Fix |
|---|---|
| Log shows `Ai-Thinker … ready` / `ERROR` | Factory AT firmware still on board; upload the firmware |
| *This chip is ESP8266, not ESP32* | Wrong board selected; pick NodeMCU 1.0 |
| Garbled text in Serial Monitor | Set baud to 115200 (garbage right after reset is normal boot output) |
| Motors hum but don't turn | Raise supply current limit / voltage, or use speed 7–9 |
| A pair won't run one direction | Check IN3/IN4 wiring to D7/D4 |
| One motor of a pair spins backwards | Swap that motor's two wires |
| `requestPort` error in browser | Open the HTML directly in Chrome, not in a preview pane |
| Board won't boot with L298N connected | Unplug IN4 (D4), press RST, reconnect |

## v2: Wi-Fi + joystick + game controller

The rover creates its own Wi-Fi network and serves the controller page itself. Nothing to install on the phone.

1. Arduino IDE → **Library Manager** → install **WebSockets** by Markus Sattler.
2. Open `firmware/rover_wifi/rover_wifi.ino` (keep `page.h` in the same folder) → board **NodeMCU 1.0** → Upload.
3. Power the rover (NodeMCU via VIN from the 5 V supply, motors from battery, common ground). USB cable no longer needed.
4. On phone/laptop: join Wi-Fi **Rover** (password `rover1234`), open **http://192.168.4.1**.
   - iPhone may warn "No Internet Connection" — tap to stay connected.

**Touch:** drag the on-screen joystick. **Game controller:** connect it to the phone/laptop, press any button, it shows as *Detected*.

| Controller mode | Drive | Steer |
|---|---|---|
| Split (default) | Left stick up/down | Right stick left/right |
| Arcade | Left stick | Left stick |
| Racing | RT forward / LT reverse | Left stick |

B = stop/resume, LB/RB = max speed −/+. If steering feels reversed, tick **Invert steering**.

Protocol (WebSocket, port 81): `d,<left>,<right>` with −255..255, sent 20×/s; `s` = stop. Motors stop after 500 ms without a command or when the page closes/disconnects.
Tuning in `rover_wifi.ino`: `MIN_PWM` (lowest power that still turns the wheels), Wi-Fi name/password.

### Controllers that work by device
| Device | Works |
|---|---|
| iPhone / Mac | Xbox Wireless, DualSense/DualShock, MFi controllers (Bluetooth) |
| Windows laptop (Chrome/Edge) | Turtle Beach Recon (wired) and Recon Cloud |
| Android (Chrome) | Recon Cloud via Bluetooth |

Turtle Beach Recon / Recon Cloud are not officially supported on iPhone or Mac — test it: if the page never says *Detected*, use one of the other combos.

## Next up
- HC-SR04 obstacle stop (TRIG D0, ECHO D8)
