#include <WiFi.h>
#include <MQTT.h>
#include <Wire.h>
#include <LCD_I2C.h>
#include "RTClib.h"

// --- กำหนดขา Pin ---
#define BUTTON_PIN 19 
#define LDR_PIN 34    
const int MOTOR_ENABLE = 23; 
const int MOTOR_INPUT1 = 22; 
const int MOTOR_INPUT2 = 21; 
const int LED_CW = 16;       
const int LED_CCW = 17;      

// --- การตั้งค่า WiFi และ MQTT ---
const char ssid[] = "@JumboPlusIoT";
const char pass[] = "07450748";
const char mqtt_broker[] = "test.mosquitto.org";
const char mqtt_client_id[] = "AI_Group4_Control";
int MQTT_PORT = 1883;

// Topics
const char topic_ldr[] = "groupAi4/ldr";
const char topic_motor_status[] = "groupAi4/motorStatus";
const char topic_mode_status[] = "groupAi4/modeStatus";
const char topic_command[] = "groupAi4/command";
const char topic_ledcw[] = "groupAi4/ledcw";
const char topic_ledccw[] = "groupAi4/ledccw";

// Objects
WiFiClient net;
MQTTClient client;
LCD_I2C lcd(0x27, 16, 2); 
RTC_DS1307 rtc;

// --- ตัวแปรสถานะ ---
bool isAutoMode = true;
unsigned long lastInterruptTime = 0;
bool motorState = false;    
bool ledcw = false;
bool ledccw = false;
int ldrValue = 0;

// Debounce และ Timing
int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;
unsigned long lastPublishTime = 0;
unsigned long lastLCDUpdateTime = 0;

void IRAM_ATTR handleButtonPress() {
  unsigned long interruptTime = millis();
  if (interruptTime - lastInterruptTime > 250) {
    isAutoMode = !isAutoMode;
    lastInterruptTime = interruptTime;
  }
}

void connect() {
  Serial.print("Connecting WiFi...");
  while (WiFi.status() != WL_CONNECTED) { delay(500); }
  
  Serial.print("\nConnecting MQTT...");
  while (!client.connect(mqtt_client_id)) { delay(500); }
  
  Serial.println("\nConnected!");
  client.subscribe(topic_command);
  publishStatus();
}

void messageReceived(String &topic, String &payload) {
  if (payload == "toggleMode") {
    isAutoMode = !isAutoMode;
  } 
  else if (!isAutoMode) { 
    if (payload == "motorOn") {
      motorState = !motorState;
    }
  }
  publishStatus();
}

void publishStatus() {
  client.publish(topic_mode_status, isAutoMode ? "Ready" : "Stop");
  client.publish(topic_motor_status, motorState ? "Working" : "Halt");
}

// --- ฟังก์ชันควบคุมมอเตอร์ ---
void rotateCW() {
  digitalWrite(MOTOR_INPUT1, HIGH);
  digitalWrite(MOTOR_INPUT2, LOW);
  digitalWrite(LED_CW, HIGH);
  ledcw = true;
  digitalWrite(LED_CCW, LOW);
  ledccw = false;
}

void rotateCCW() {
  digitalWrite(MOTOR_INPUT1, LOW);
  digitalWrite(MOTOR_INPUT2, HIGH);
  digitalWrite(LED_CW, LOW);
  ledcw = false;
  digitalWrite(LED_CCW, HIGH);
  ledccw = true;
}

void stopMotor() {
  digitalWrite(MOTOR_INPUT1, LOW);
  digitalWrite(MOTOR_INPUT2, LOW);
  digitalWrite(LED_CW, LOW);
  digitalWrite(LED_CCW, LOW);
  ledcw = false;
  ledccw = false;
}

void updateLCD() {
  lcd.clear();
  // แถวที่ 1: Mode และค่า LDR
  lcd.setCursor(0, 0);
  lcd.print("M:"); 
  lcd.print(isAutoMode ? "AUTO " : "MANU ");
  lcd.print("LDR:");
  lcd.print(ldrValue);

  // แถวที่ 2: สถานะ Motor
  lcd.setCursor(0, 1);
  lcd.print("Motor: ");
  if (!motorState) {
    lcd.print("STOP");
  } else {
    lcd.print(ledcw ? "CW >>" : "CCW <<");
  }
}

void setup() {
  Serial.begin(115200);
  
  // LCD Setup
  lcd.begin();
  lcd.backlight();
  lcd.print("System Loading...");

  // RTC Setup
  if (!rtc.begin()) {
    Serial.println("RTC Error!");
  }
  if (!rtc.isrunning()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonPress, FALLING);
  
  pinMode(MOTOR_ENABLE, OUTPUT);
  pinMode(MOTOR_INPUT1, OUTPUT);
  pinMode(MOTOR_INPUT2, OUTPUT);
  pinMode(LED_CW, OUTPUT);
  pinMode(LED_CCW, OUTPUT);
  
  digitalWrite(MOTOR_ENABLE, HIGH);
  stopMotor();

  WiFi.begin(ssid, pass);
  client.begin(mqtt_broker, MQTT_PORT, net);
  client.onMessage(messageReceived);

  connect();
  lcd.clear();
}

void loop() {
  client.loop();
  if (!client.connected()) connect();

  // 1. อ่านค่าเซ็นเซอร์และส่ง MQTT ทุก 1 วินาที
  if (millis() - lastPublishTime > 1000) {
    ldrValue = analogRead(LDR_PIN);
    client.publish(topic_ldr, String(ldrValue));
    publishStatus();
    lastPublishTime = millis();
  }

  // 2. ตรรกะปุ่มกด (Physical Button) พร้อม Debounce
  int reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonState) lastDebounceTime = millis();
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading == LOW && lastButtonState == HIGH) {
      isAutoMode = !isAutoMode;
      publishStatus();
    }
  }
  lastButtonState = reading;

  // 3. ควบคุมมอเตอร์ตามโหมด
  if (isAutoMode) {
    if (ldrValue > 500) { rotateCW(); motorState = true; }
    else if (ldrValue > 300 && ldrValue <= 500) { rotateCCW(); motorState = true; }
    else { stopMotor(); motorState = false; }
  } else {
    // ในโหมด Manual ใช้ค่า motorState จากคำสั่ง MQTT
    if (!motorState) stopMotor();
    // (เพิ่มเติม: หากต้องการให้ Manual หมุนทิศทางไหน สามารถเพิ่มเงื่อนไขจาก MQTT ได้)
  }

  // 4. อัปเดตหน้าจอ LCD ทุก 500ms เพื่อความลื่นไหล
  if (millis() - lastLCDUpdateTime > 500) {
    updateLCD();
    lastLCDUpdateTime = millis();
  }
}
