// Rover firmware v2: NodeMCU ESP8266 + L298N, Wi-Fi joystick control
// - Rover creates its own Wi-Fi network (AP). Join it, open http://192.168.4.1
// - Page: touch joystick + game controller (Gamepad API), proportional speed
// - Commands over WebSocket (port 81):  "d,<left>,<right>"  (-255..255)  |  "s" stop
// - USB serial still accepts the old single-char commands (F/B/L/R/S, 0-9)
// - Failsafe: motors stop after 500 ms without a command, or when a client disconnects
//
// Board:   Tools > Board > esp8266 > NodeMCU 1.0 (ESP-12E Module)
// Library: Library Manager > "WebSockets" by Markus Sattler

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include "page.h"

// ---- Wi-Fi network the rover creates (password: 8+ characters) ----
const char* AP_SSID = "Rover";
const char* AP_PASS = "rover1234";

// ---- L298N pins (matches team wiring diagram; D0/D8 reserved for HC-SR04) ----
const int ENA = 5;   // [D1]
const int IN1 = 14;  // [D5]
const int IN2 = 12;  // [D6]
const int ENB = 4;   // [D2]
const int IN3 = 13;  // [D7]
const int IN4 = 2;   // [D4]

// ---- Tuning ----
const int MIN_PWM = 80;                 // lowest PWM that still turns the wheels
const unsigned long FAILSAFE_MS = 500;

ESP8266WebServer http(80);
WebSocketsServer ws(81);

unsigned long lastCmd = 0;
bool moving = false;
int serialSpeed = 180;
char serialState = 'S';

// v: -255..255. Any non-zero value is scaled into MIN_PWM..255 so small stick moves still turn.
void setMotor(int en, int a, int b, int v) {
  if (v == 0) {
    digitalWrite(a, LOW);
    digitalWrite(b, LOW);
    analogWrite(en, 0);
    return;
  }
  digitalWrite(a, v > 0 ? HIGH : LOW);
  digitalWrite(b, v < 0 ? HIGH : LOW);
  int mag = abs(v);
  if (mag > 255) mag = 255;
  analogWrite(en, map(mag, 1, 255, MIN_PWM, 255));
}

// Physical mapping found in testing: right pair = channel A (OUT1/2),
// left pair = channel B (OUT3/4) with reversed polarity. If wiring changes, edit only these two lines.
void sides(int left, int right) {
  setMotor(ENA, IN1, IN2, right);
  setMotor(ENB, IN3, IN4, -left);
}

void drive(int left, int right) {
  left = constrain(left, -255, 255);
  right = constrain(right, -255, 255);
  sides(left, right);
  moving = (left != 0 || right != 0);
  lastCmd = millis();
}

void stopAll() {
  sides(0, 0);
  moving = false;
  serialState = 'S';
}

void onWs(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      ws.sendTXT(num, "READY");
      Serial.printf("Client %u connected\n", num);
      break;
    case WStype_DISCONNECTED:
      stopAll();
      Serial.printf("Client %u disconnected, stopped\n", num);
      break;
    case WStype_TEXT: {
      char buf[32];
      size_t n = length < sizeof(buf) - 1 ? length : sizeof(buf) - 1;
      memcpy(buf, payload, n);
      buf[n] = '\0';
      int l, r;
      if (buf[0] == 'd' && sscanf(buf + 2, "%d,%d", &l, &r) == 2) {
        drive(l, r);
      } else if (buf[0] == 's') {
        stopAll();
      }
      break;
    }
    default:
      break;
  }
}

void handleSerial() {
  while (Serial.available()) {
    char c = toupper(Serial.read());
    if (c >= '0' && c <= '9') {
      serialSpeed = map(c - '0', 0, 9, 0, 255);
    } else if (c == 'F' || c == 'B' || c == 'L' || c == 'R' || c == 'S') {
      if (c != serialState) { Serial.print("CMD "); Serial.println(c); }
      serialState = c;
    } else {
      continue;
    }
    int s = serialSpeed;
    switch (serialState) {
      case 'F': drive( s,  s); break;
      case 'B': drive(-s, -s); break;
      case 'L': drive( s, -s); break;   // A: left pair fwd, right pair back
      case 'R': drive(-s,  s); break;   // D: right pair fwd, left pair back
      default:  stopAll();
    }
  }
}

void setup() {
  Serial.begin(115200);
  int pins[] = {ENA, IN1, IN2, ENB, IN3, IN4};
  for (int p : pins) pinMode(p, OUTPUT);
  stopAll();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);

  http.on("/", []() { http.send_P(200, "text/html", PAGE); });
  http.onNotFound([]() { http.sendHeader("Location", "/"); http.send(302); });
  http.begin();

  ws.begin();
  ws.onEvent(onWs);

  Serial.println();
  Serial.println("ROVER READY");
  Serial.print("Wi-Fi: "); Serial.print(AP_SSID);
  Serial.print("  password: "); Serial.println(AP_PASS);
  Serial.print("Open http://"); Serial.println(WiFi.softAPIP());
}

void loop() {
  http.handleClient();
  ws.loop();
  handleSerial();
  if (moving && millis() - lastCmd > FAILSAFE_MS) {
    stopAll();
    Serial.println("FAILSAFE STOP");
  }
}
