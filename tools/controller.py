#!/usr/bin/env python3
"""Mac keyboard controller for the rover.
   pip3 install pyserial
   python3 controller.py            (auto-detects the CP2102 port)
   python3 controller.py /dev/cu.usbserial-0001

Keys: W/↑ fwd  S/↓ back  A/← left  D/→ right  SPACE stop  0-9 speed  Q quit
A command stays active until you press another key or SPACE.
"""
import curses, glob, sys, time
import serial

KEYS = {
    ord('w'): 'F', curses.KEY_UP: 'F',
    ord('s'): 'B', curses.KEY_DOWN: 'B',
    ord('a'): 'L', curses.KEY_LEFT: 'L',
    ord('d'): 'R', curses.KEY_RIGHT: 'R',
    ord(' '): 'S',
}
NAMES = {'F': 'FORWARD', 'B': 'BACKWARD', 'L': 'LEFT', 'R': 'RIGHT', 'S': 'STOPPED'}
HEARTBEAT = 0.1  # seconds; firmware stops after 0.5 s of silence


def find_port():
    ports = glob.glob('/dev/cu.SLAB_USBtoUART*') + glob.glob('/dev/cu.usbserial*')
    if not ports:
        sys.exit("No CP2102 port found. Run: ls /dev/cu.*  (install SiLabs VCP driver if nothing shows)")
    return ports[0]


def main(scr, port):
    ser = serial.Serial(port, 115200, timeout=0)
    time.sleep(2)  # ESP32 resets when the port opens
    ser.reset_input_buffer()
    curses.curs_set(0)
    scr.nodelay(True)
    cmd, speed, last_send, log = 'S', 7, 0, ''
    ser.write(str(speed).encode())

    while True:
        k = scr.getch()
        if k in (ord('q'), ord('Q')):
            break
        if k in KEYS:
            cmd = KEYS[k]
        elif ord('0') <= k <= ord('9'):
            speed = k - ord('0')
            ser.write(chr(k).encode())

        now = time.time()
        if now - last_send >= HEARTBEAT or k in KEYS:
            ser.write(cmd.encode())
            last_send = now

        line = ser.readline().decode(errors='ignore').strip()
        if line:
            log = line

        scr.erase()
        scr.addstr(0, 0, f"Rover on {port}")
        scr.addstr(2, 0, f"State: {NAMES[cmd]:<10}  Speed: {speed}/9")
        scr.addstr(4, 0, "W/↑ fwd  S/↓ back  A/← left  D/→ right  SPACE stop  0-9 speed  Q quit")
        scr.addstr(6, 0, f"Rover says: {log}")
        scr.refresh()
        time.sleep(0.02)

    ser.write(b'S')
    ser.close()


if __name__ == '__main__':
    port = sys.argv[1] if len(sys.argv) > 1 else find_port()
    curses.wrapper(main, port)
