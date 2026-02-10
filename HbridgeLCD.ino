#include <WiFi.h>
#include <MQTT.h>
#include <Wire.h>
#include <LCD_I2C.h>
#include "RTClib.h"

// --- Pin Config ---
#define BUTTON_PIN 19 
#define LDR_PIN 34    
const int MOTOR_ENABLE = 23; 
const int MOTOR_INPUT1 = 16; 
const int MOTOR_INPUT2 = 17; 
const int LED_CW = 16;       
const int LED_CCW = 17;      

// --- WiFi & MQTT ---
const char ssid[] = "@JumboPlusIoT";
const char pass[] = "07450748";
const char mqtt_broker[] = "test.mosquitto.org";
const char mqtt_client_id[] = "AI_Group4_Interrupt_Control";
int MQTT_PORT = 1883;

WiFiClient net;
MQTTClient client;
LCD_I2C lcd(0x27, 16, 2); 
RTC_DS1307 rtc;

// --- State Variables ---
volatile bool isAutoMode = true; // ใช้ volatile สำหรับตัวแปรที่ใช้ใน Interrupt
bool motorState = false;    
bool ledcw = false;
bool ledccw = false;
int ldrValue = 0;

unsigned long lastPublishTime = 0;
unsigned long lastLCDUpdateTime = 0;
volatile unsigned long lastInterruptTime = 0; // สำหรับ Debounce ใน Interrupt

// --- Interrupt Service Routine (ISR) ---
void IRAM_ATTR handleButtonPress() {
  unsigned long interruptTime = millis();
  // Debounce 250ms: ถ้ากดย้ำๆ ในช่วงเวลานี้จะไม่ทำงาน
  if (interruptTime - lastInterruptTime > 250) {
    isAutoMode = !isAutoMode;
    lastInterruptTime = interruptTime;
  }
}

// --- ฟังก์ชันควบคุมมอเตอร์ ---
void stopMotor() {
  digitalWrite(MOTOR_INPUT1, LOW);
  digitalWrite(MOTOR_INPUT2, LOW);
  digitalWrite(LED_CW, LOW);
  digitalWrite(LED_CCW, LOW);
  ledcw = false; ledccw = false; motorState = false;
}

void rotateCW() {
  digitalWrite(MOTOR_INPUT1, HIGH);
  digitalWrite(MOTOR_INPUT2, LOW);
  digitalWrite(LED_CW, HIGH);
  digitalWrite(LED_CCW, LOW);
  ledcw = true; ledccw = false; motorState = true;
}

void rotateCCW() {
  digitalWrite(MOTOR_INPUT1, LOW);
  digitalWrite(MOTOR_INPUT2, HIGH);
  digitalWrite(LED_CW, LOW);
  digitalWrite(LED_CCW, HIGH);
  ledcw = false; ledccw = true; motorState = true;
}

void publishStatus() {
  if (client.connected()) {
    client.publish("groupAi4/modeStatus", isAutoMode ? "Auto" : "Manual");
    client.publish("groupAi4/motorStatus", motorState ? "Working" : "Stop");
  }
}

void connect() {
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 15) {
    delay(500);
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED && client.connect(mqtt_client_id)) {
    client.subscribe("groupAi4/command");
    publishStatus();
  }
}

void messageReceived(String &topic, String &payload) {
  if (payload == "toggleMode") isAutoMode = !isAutoMode;
  else if (!isAutoMode) {
    if (payload == "motorOn") motorState = true;
    if (payload == "motorOff") motorState = false;
  }
}

void setup() {
  Serial.begin(115200);
  
  Wire.begin(); 
  lcd.begin();
  lcd.backlight();
  lcd.print("System Ready...");

  if (!rtc.begin()) Serial.println("RTC Not Found");

  // --- Setup Interrupt ---
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  // สั่งให้เรียก handleButtonPress เมื่อแรงดันขา 19 เปลี่ยนจาก High เป็น Low (FALLING)
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
  
  if (WiFi.status() == WL_CONNECTED && !client.connected()) {
    static unsigned long lastRetry = 0;
    if (millis() - lastRetry > 5000) {
      client.connect(mqtt_client_id);
      lastRetry = millis();
    }
  }

  ldrValue = analogRead(LDR_PIN);

  // ตรรกะควบคุมมอเตอร์ (อิงตามโหมดที่ถูกเปลี่ยนจาก Interrupt)
  if (isAutoMode) {
    if (ldrValue > 700) rotateCW();
    else if (ldrValue > 300) rotateCCW();
    else stopMotor();
  } else {
    if (!motorState) stopMotor(); 
    else if (!ledcw && !ledccw) rotateCW(); // Manual Default ให้หมุน CW
  }

  // ส่งข้อมูล MQTT ทุก 2 วินาที
  if (millis() - lastPublishTime > 2000) {
    client.publish("groupAi4/ldr", String(ldrValue));
    publishStatus();
    lastPublishTime = millis();
  }

  // อัปเดต LCD
  if (millis() - lastLCDUpdateTime > 500) {
    lcd.setCursor(0, 0);
    lcd.print("MOD:"); lcd.print(isAutoMode ? "AUTO " : "MANU ");
    lcd.print("LDR:"); lcd.print(ldrValue);
    lcd.print("    ");

    lcd.setCursor(0, 1);
    lcd.print("MOT:");
    if (!motorState) lcd.print("STOP      ");
    else lcd.print(ledcw ? "RUN [CW]  " : "RUN [CCW] ");
    
    lastLCDUpdateTime = millis();
  }
}
