#include <WiFi.h>
#include <MQTT.h>
#include <Stepper.h>
#include <Wire.h>
#include <LCD_I2C.h>
#include "RTClib.h"

// --- 1. Config WiFi & MQTT ---
const char ssid[] = "YOUR_WIFI_SSID";
const char pass[] = "YOUR_WIFI_PASS";
const char mqtt_broker[] = "broker.hivemq.com";
const char mqtt_client_id[] = "esp32_stepper_exam_01";
String topic_prefix = "my_exam_user1"; 

// --- 2. Pins & Hardware Config ---
const int LDR_PIN = 34;
const int STEPS_PER_REV = 2048; 
const int IN1 = 26;
const int IN2 = 27;
const int IN3 = 14;
const int IN4 = 12;

Stepper myStepper(STEPS_PER_REV, IN1, IN3, IN2, IN4);
LCD_I2C lcd(0x27, 16, 2); 
RTC_DS1307 rtc;

// --- 3. Variables ---
WiFiClient net;
MQTTClient client;
unsigned long lastMillis = 0;
float currentAngle = 0.0;
int ldrValueMapped = 0;

// --- Functions ---
void connect() {
  Serial.print("Checking WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.print("\nMQTT Connecting...");
  while (!client.connect(mqtt_client_id)) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nConnected!");
  client.subscribe(topic_prefix + "/angle/reset");
}

void messageReceived(String &topic, String &payload) {
  if (topic.endsWith("/angle/reset")) {
    currentAngle = 0.0;
    client.publish(topic_prefix + "/angle", String(currentAngle));
    Serial.println("Angle Reset");
  }
}

void setup() {
  Serial.begin(115200);

  // Setup LCD & RTC
  lcd.begin();
  lcd.backlight();
  if (!rtc.begin()) {
    lcd.print("RTC Error!");
    while (1);
  }
  if (!rtc.isrunning()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // Setup Stepper
  myStepper.setSpeed(10);
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(LDR_PIN, INPUT);

  WiFi.begin(ssid, pass);
  client.begin(mqtt_broker, 1883, net);
  client.onMessage(messageReceived);
  connect();
  lcd.clear();
}

void loop() {
  client.loop();
  if (!client.connected()) connect();

  // 1. อ่านค่า LDR
  int ldrValueRaw = analogRead(LDR_PIN);
  ldrValueMapped = map(ldrValueRaw, 0, 4095, 0, 1023);

  // 2. ควบคุมมอเตอร์ (หมุนเมื่อค่าแสง > 500)
  if (ldrValueMapped > 500) {
    int stepsToMove = 32; 
    myStepper.step(stepsToMove);
    currentAngle += (float)stepsToMove / STEPS_PER_REV * 360.0;
  } else {
    // หยุดจ่ายไฟให้ขดลวดเมื่อไม่หมุนเพื่อลดความร้อน
    digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
    digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
  }

  // 3. แสดงผลหน้าจอ LCD และส่ง MQTT ทุก 1 วินาที
  if (millis() - lastMillis > 1000) {
    lastMillis = millis();
    DateTime now = rtc.now();

    // --- LCD แถวที่ 1: แสดงเฉพาะเวลา ---
    lcd.setCursor(0, 0);
    lcd.print("Time: ");
    if(now.hour() < 10) lcd.print('0'); lcd.print(now.hour());
    lcd.print(':');
    if(now.minute() < 10) lcd.print('0'); lcd.print(now.minute());
    lcd.print(':');
    if(now.second() < 10) lcd.print('0'); lcd.print(now.second());
    lcd.print("    "); // ล้างเศษตัวอักษร

    // --- LCD แถวที่ 2: LDR และ องศาสะสม ---
    lcd.setCursor(0, 1);
    lcd.print("L:"); lcd.print(ldrValueMapped);
    lcd.setCursor(7, 1);
    lcd.print("Deg:"); lcd.print(currentAngle, 1); 
    lcd.print("    "); 

    // 4. ส่งค่าขึ้น MQTT
    client.publish(topic_prefix + "/ldr", String(ldrValueMapped));
    client.publish(topic_prefix + "/angle", String(currentAngle));
  }
}
