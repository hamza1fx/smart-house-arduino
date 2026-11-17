/*
 * Smart House – Arduino Automation & Safety System
 * ENGG200 Introduction to Engineering, Lebanese International University
 *
 * Features:
 *  - Automatic lighting (LDR)
 *  - Gas-leak and fire/over-temperature detection -> alarm + window opens
 *  - Intruder detection (PIR) in Sleep mode -> alarm (window stays closed)
 *  - Three operating modes: NORMAL, SLEEP, EMERGENCY
 *  - Push button: NORMAL <-> SLEEP, and resets an intruder alarm
 *
 * Simulated in Tinkercad (Arduino Uno).
 */

#include <Servo.h>

// ---------------- Pins ----------------
const int LDR_PIN    = A2;
const int TEMP_PIN   = A1;   // TMP36 temperature sensor
const int GAS_PIN    = A0;
const int PIR_PIN    = 8;
const int BUTTON_PIN = 6;

const int LED1_PIN   = 12;
const int LED2_PIN   = 13;
const int LED3_PIN   = 2;
const int RGB_R_PIN  = 9;
const int RGB_G_PIN  = 11;
const int RGB_B_PIN  = 10;
const int BUZZER_PIN = 7;
const int SERVO_PIN  = 3;

// ---------------- Settings ----------------
const int   LDR_THRESHOLD  = 100;   // below this = dark -> lights on
const float TEMP_THRESHOLD = 50.0;  // degrees C
const int   GAS_THRESHOLD  = 260;   // raw ADC value
const int   WINDOW_OPEN    = 90;    // servo angle
const int   WINDOW_CLOSED  = 0;
const unsigned long BLINK_MS = 500;

// ---------------- State ----------------
enum Mode { NORMAL, SLEEP, EMERGENCY };
Mode currentMode = NORMAL;

bool gasAlarm    = false;
bool tempAlarm   = false;
bool motionAlarm = false;   // latched until reset with the button

Servo windowServo;
unsigned long lastBlink = 0;
bool redState = false;
bool lastButton = HIGH;

void setup() {
  pinMode(LDR_PIN, INPUT);
  pinMode(TEMP_PIN, INPUT);
  pinMode(GAS_PIN, INPUT);
  pinMode(PIR_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  pinMode(LED3_PIN, OUTPUT);
  pinMode(RGB_R_PIN, OUTPUT);
  pinMode(RGB_G_PIN, OUTPUT);
  pinMode(RGB_B_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  windowServo.attach(SERVO_PIN);
  Serial.begin(9600);
}

void loop() {
  // ---- Read sensors ----
  int ldrValue = analogRead(LDR_PIN);
  int gasValue = analogRead(GAS_PIN);
  int motion   = digitalRead(PIR_PIN);
  float temperature = readTemperatureC();

  // ---- Automatic alarms ----
  gasAlarm  = (gasValue > GAS_THRESHOLD);
  tempAlarm = (temperature > TEMP_THRESHOLD);

  // ---- Lighting ----
  setLights(ldrValue < LDR_THRESHOLD);

  // ---- Button ----
  handleButton();

  // ---- Intruder detection (only in Sleep mode) ----
  if (currentMode == SLEEP && motion == HIGH) {
    motionAlarm = true;
  }

  // ---- Mode decision ----
  if (gasAlarm || tempAlarm || motionAlarm) {
    currentMode = EMERGENCY;
  } else if (currentMode == EMERGENCY) {
    currentMode = NORMAL;
  }

  // ---- Window: open only for gas or fire, not for intruders ----
  windowServo.write((gasAlarm || tempAlarm) ? WINDOW_OPEN : WINDOW_CLOSED);

  // ---- Alarm outputs ----
  if (currentMode == EMERGENCY) {
    digitalWrite(BUZZER_PIN, HIGH);
    blinkRed();
  } else {
    digitalWrite(BUZZER_PIN, LOW);
    setRGB(LOW, LOW, LOW);
  }

  printStatus(gasValue, temperature);
  delay(100);
}

// ---------------- Helpers ----------------

// TMP36: 10 mV per degree C with a 500 mV offset
float readTemperatureC() {
  float voltage = analogRead(TEMP_PIN) * (5.0 / 1023.0);
  return (voltage - 0.5) * 100.0;
}

void setLights(bool on) {
  digitalWrite(LED1_PIN, on);
  digitalWrite(LED2_PIN, on);
  digitalWrite(LED3_PIN, on);
}

void setRGB(int r, int g, int b) {
  digitalWrite(RGB_R_PIN, r);
  digitalWrite(RGB_G_PIN, g);
  digitalWrite(RGB_B_PIN, b);
}

void blinkRed() {
  unsigned long now = millis();
  if (now - lastBlink >= BLINK_MS) {
    lastBlink = now;
    redState = !redState;
  }
  setRGB(redState, LOW, LOW);
}

// Button press (falling edge):
//  NORMAL -> SLEEP
//  SLEEP  -> NORMAL
//  EMERGENCY caused by an intruder -> reset alarm, back to SLEEP
void handleButton() {
  bool button = digitalRead(BUTTON_PIN);
  if (lastButton == HIGH && button == LOW) {
    if (currentMode == NORMAL) {
      currentMode = SLEEP;
    } else if (currentMode == SLEEP) {
      currentMode = NORMAL;
    } else if (currentMode == EMERGENCY && motionAlarm) {
      motionAlarm = false;
      currentMode = SLEEP;
    }
    delay(300);  // simple debounce
  }
  lastButton = button;
}

void printStatus(int gasValue, float temperature) {
  const char* names[] = { "NORMAL", "SLEEP", "EMERGENCY" };
  Serial.print("Mode: ");        Serial.print(names[currentMode]);
  Serial.print(" | Gas: ");      Serial.print(gasValue);
  Serial.print(" | Temp: ");     Serial.print(temperature);
  Serial.print(" C | Intruder: "); Serial.println(motionAlarm ? "YES" : "no");
}
