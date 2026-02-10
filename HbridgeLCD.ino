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
const int LED_CW = 4;       
const int LED_CCW = 5;      

// --- WiFi & MQTT ---
const char ssid[] = "@JumboPlusIoT";
const char pass[] = "07450748";
const char mqtt_broker[] = "test.mosquitto.org";
const char mqtt_client_id[] = "AI_Group4_Control_New"; // เปลี่ยน ID เผื่อซ้ำ
int MQTT_PORT = 1883;

WiFiClient net;
MQTTClient client;
LCD_I2C lcd(0x27, 16, 2); // ถ้ายังค้าง ลองเปลี่ยนเป็น 0x3F
RTC_DS1307 rtc;

// --- State Variables ---
bool isAutoMode = true;
bool motorState = false;    
bool ledcw = false;
bool ledccw = false;
int ldrValue = 0;

unsigned long lastPublishTime = 0;
unsigned long lastLCDUpdateTime = 0;

// --- Functions ---
void publishStatus() {
  if (client.connected()) {
    client.publish("groupAi4/modeStatus", isAutoMode ? "Auto" : "Manual");
    client.publish("groupAi4/motorStatus", motorState ? "Working" : "Stop");
  }
}

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

void connect() {
  // เชื่อมต่อ WiFi แบบจำกัดเวลา (ไม่ค้างตลอดไป)
  int attempts = 0;
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    if (client.connect(mqtt_client_id)) {
      client.subscribe("groupAi4/command");
      publishStatus();
    }
  } else {
    Serial.println("\nWiFi Failed (Skip)");
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
  
  // 1. เริ่มต้น LCD ก่อนเพื่อน
  Wire.begin(); 
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.print("Initializing...");

  // 2. เริ่มต้น RTC (ถ้าไม่เจอก็แค่แจ้ง Serial ไม่ให้ค้าง)
  if (!rtc.begin()) {
    Serial.println("RTC Not Found");
  }

  // 3. Setup Pins
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(MOTOR_ENABLE, OUTPUT);
  pinMode(MOTOR_INPUT1, OUTPUT);
  pinMode(MOTOR_INPUT2, OUTPUT);
  pinMode(LED_CW, OUTPUT);
  pinMode(LED_CCW, OUTPUT);
  digitalWrite(MOTOR_ENABLE, HIGH);
  stopMotor();

  // 4. WiFi & MQTT
  WiFi.begin(ssid, pass);
  client.begin(mqtt_broker, MQTT_PORT, net);
  client.onMessage(messageReceived);
  
  connect();
  lcd.clear();
}

void loop() {
  client.loop();
  
  // Reconnect MQTT ถ้าหลุด (แบบไม่ดึงจังหวะ loop)
  if (WiFi.status() == WL_CONNECTED && !client.connected()) {
    static unsigned long lastRetry = 0;
    if (millis() - lastRetry > 5000) {
      client.connect(mqtt_client_id);
      lastRetry = millis();
    }
  }

  // อ่านค่า LDR
  ldrValue = analogRead(LDR_PIN);

  // ตรรกะปุ่มกด (สลับโหมด)
  static int lastBtn = HIGH;
  int currBtn = digitalRead(BUTTON_PIN);
  if (lastBtn == HIGH && currBtn == LOW) {
    isAutoMode = !isAutoMode;
    delay(50); // Simple debounce
  }
  lastBtn = currBtn;

  // ควบคุมมอเตอร์
  if (isAutoMode) {
    if (ldrValue > 700) rotateCW();
    else if (ldrValue > 300) rotateCCW();
    else stopMotor();
  } else {
    // ใน Manual Mode ใช้คำสั่งจาก MQTT motorState
    if (!motorState) stopMotor(); 
    else {
       // ถ้า Manual ON ให้หมุน CW เป็นค่าเริ่มต้น
       if (!ledcw && !ledccw) rotateCW(); 
    }
  }

  // ส่งข้อมูล MQTT ทุก 2 วินาที
  if (millis() - lastPublishTime > 2000) {
    client.publish("groupAi4/ldr", String(ldrValue));
    publishStatus();
    lastPublishTime = millis();
  }

  // อัปเดต LCD ทุก 500ms
  if (millis() - lastLCDUpdateTime > 500) {
    lcd.setCursor(0, 0);
    lcd.print("MOD:"); lcd.print(isAutoMode ? "AUTO " : "MANU ");
    lcd.print("LDR:"); lcd.print(ldrValue);
    lcd.print("    "); // Clear ท้ายบรรทัด

    lcd.setCursor(0, 1);
    lcd.print("MOT:");
    if (!motorState) lcd.print("STOP      ");
    else lcd.print(ledcw ? "RUN [CW]  " : "RUN [CCW] ");
    
    lastLCDUpdateTime = millis();
  }
}
