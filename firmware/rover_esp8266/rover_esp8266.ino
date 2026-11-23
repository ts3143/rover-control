// Rover firmware: ESP8266 (NodeMCU) + L298N, commands over USB serial (CP2102)
// Board: Tools > Board > esp8266 > "NodeMCU 1.0 (ESP-12E Module)"
// Commands (single chars): F=forward B=back L=left R=right S=stop, 0-9 = speed
// Analog (gamepad): "M<left> <right>\n", each -255..255 PWM, e.g. "M200 -120\n"
// Failsafe: motors stop if no command for FAILSAFE_MS.

// ---- L298N pins (GPIO numbers; NodeMCU label in brackets) ----
// Matches the team wiring diagram. D0/D8 are reserved for the HC-SR04 (TRIG/ECHO).
// Remove the ENA/ENB jumpers on the L298N to use PWM speed control.
// OUT1/OUT2 = left pair (parallel), OUT3/OUT4 = right pair (parallel).
const int ENA = 5;   // [D1] left motors speed (PWM)
const int IN1 = 14;  // [D5]
const int IN2 = 12;  // [D6]
const int ENB = 4;   // [D2] right motors speed (PWM)
const int IN3 = 13;  // [D7]
const int IN4 = 2;   // [D4] (also the on-board LED, so it will blink while driving)

const unsigned long FAILSAFE_MS = 500;
int speedPWM = 180;            // 0-255
unsigned long lastCmd = 0;
char state = 'S';

// pwm: -255..255 (sign = direction, 0 = stop)
void setMotor(int en, int a, int b, int pwm) {
  digitalWrite(a, pwm > 0 ? HIGH : LOW);
  digitalWrite(b, pwm < 0 ? HIGH : LOW);
  analogWrite(en, abs(pwm));
}

// Physical mapping found in testing: right pair = channel A (OUT1/2),
// left pair = channel B (OUT3/4) with reversed polarity.
// If wiring changes, only edit these two lines.
// left/right: -255..255 PWM
void sides(int left, int right) {
  setMotor(ENA, IN1, IN2,  right);
  setMotor(ENB, IN3, IN4, -left);
}

void drive(char c) {
  int v = speedPWM;
  switch (c) {
    case 'F': sides( v,  v); break;  // W: all forward
    case 'B': sides(-v, -v); break;  // S: all backward
    case 'L': sides( v, -v); break;  // A: left pair fwd, right pair back
    case 'R': sides(-v,  v); break;  // D: right pair fwd, left pair back
    default:  sides( 0,  0); c = 'S';
  }
  state = c;
}

// "M<left> <right>" from the gamepad: proportional speed per side
char mbuf[24];
int mlen = -1;  // -1 = not reading an M line

void analogDrive(const char *s) {
  int l = 0, r = 0;
  if (sscanf(s, "%d %d", &l, &r) != 2) return;
  l = constrain(l, -255, 255);
  r = constrain(r, -255, 255);
  sides(l, r);
  char prev = state;
  state = (l == 0 && r == 0) ? 'S' : 'M';
  if (state != prev) { Serial.print("CMD "); Serial.println(state); }
  lastCmd = millis();
}

void setup() {
  Serial.begin(115200);
  int pins[] = {ENA, IN1, IN2, ENB, IN3, IN4};
  for (int p : pins) pinMode(p, OUTPUT);
  drive('S');
  Serial.println();
  Serial.println("ROVER READY");
}

void loop() {
  while (Serial.available()) {
    char c = toupper(Serial.read());
    if (mlen >= 0) {                       // inside an M line
      if (c == '\n' || c == '\r') { mbuf[mlen] = 0; mlen = -1; analogDrive(mbuf); }
      else if (mlen < (int)sizeof(mbuf) - 1) mbuf[mlen++] = c;
      else mlen = -1;                      // too long: drop it
      continue;
    }
    if (c == 'M') { mlen = 0; continue; }
    if (c >= '0' && c <= '9') {
      speedPWM = map(c - '0', 0, 9, 0, 255);
      if (state != 'M') drive(state);
    } else if (c == 'F' || c == 'B' || c == 'L' || c == 'R' || c == 'S') {
      if (c != state) { drive(c); Serial.print("CMD "); Serial.println(c); }
    } else {
      continue;
    }
    lastCmd = millis();
  }
  if (state != 'S' && millis() - lastCmd > FAILSAFE_MS) {
    drive('S');
    Serial.println("FAILSAFE STOP");
  }
  yield();
}
