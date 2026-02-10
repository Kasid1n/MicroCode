#include <Wire.h>
#include <LCD_I2C.h>
#include "RTClib.h"
#include <WiFi.h>
#include <MQTT.h>
#include <AccelStepper.h>

// --- ตั้งค่าอุปกรณ์ ---
LCD_I2C lcd(0x27, 16, 2); 
RTC_DS1307 rtc;
#define LDR_PIN 34 

// พินสำหรับ ULN2003 (IN1, IN3, IN2, IN4)
AccelStepper stepper(AccelStepper::FULL4WIRE, 23, 19, 22, 18);

// --- การตั้งค่า WiFi และ MQTT ---
const char ssid[] = "@JumboPlusIoT";
const char pass[] = "07450748";
const char mqtt_broker[] = "test.mosquitto.org";
const char mqtt_client_id[] = "AI_Group4_Final_System";

const char topic_ldr[] = "groupAi4/ldr";
const char topic_degree[] = "groupAi4/degree";
const char topic_motor_status[] = "groupAi4/motorStatus";
const char topic_command[] = "groupAi4/command"; 

WiFiClient net;
MQTTClient client;

// --- ตัวแปรสถาณะ ---
const int stepsPerRevolution = 2048; 
bool isEmergency = false; 
int ldrValue = 0;
float currentDeg = 0;
unsigned long lastReportTime = 0;
char daysOfTheWeek[7][12] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

// --- ฟังก์ชันเชื่อมต่อ WiFi & MQTT ---
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  Serial.print("\nConnecting to WiFi...");
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
}

void connectMQTT() {
  if (WiFi.status() != WL_CONNECTED) return;
  while (!client.connect(mqtt_client_id)) {
    Serial.print(".");
    delay(1000);
  }
  client.subscribe(topic_command);
  Serial.println("\nMQTT Connected!");
}

void messageReceived(String &topic, String &payload) {
  if (payload == "lock" || payload == "emergency") {
    isEmergency = !isEmergency;
    if (isEmergency) stepper.stop();
  } else if (payload == "reset_zero") {
    stepper.moveTo(0);
  }
}

void setup() {
  Serial.begin(115200);
  
  // เริ่มต้น LCD
  lcd.begin();
  lcd.backlight();
  lcd.print("System Loading...");

  // เริ่มต้น RTC
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    lcd.clear(); lcd.print("RTC Error!");
  }
  if (!rtc.isrunning()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // เริ่มต้น Stepper
  stepper.setMaxSpeed(1000);
  stepper.setAcceleration(800);
  stepper.setCurrentPosition(0);

  // เชื่อมต่อ Network
  connectWiFi();
  client.begin(mqtt_broker, net);
  client.onMessage(messageReceived);
  connectMQTT();
  
  lcd.clear();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWiFi();
  if (!client.connected()) connectMQTT();
  client.loop();

  DateTime now = rtc.now();
  ldrValue = analogRead(LDR_PIN);
  
  // คำนวณองศา 0-359
  currentDeg = fmod((float)stepper.currentPosition() / stepsPerRevolution * 360.0, 360.0);
  if (currentDeg < 0) currentDeg += 360.0;

  // --- การแสดงผลบน LCD ---
  if (millis() - lastReportTime > 500) { // อัปเดตจอทุก 0.5 วินาที
    // แถวที่ 1: เวลา
    lcd.setCursor(0, 0);
    if(now.hour() < 10) lcd.print('0'); lcd.print(now.hour());
    lcd.print(':');
    if(now.minute() < 10) lcd.print('0'); lcd.print(now.minute());
    lcd.print(" ");
    lcd.print(daysOfTheWeek[now.dayOfTheWeek()]);
    lcd.print("         "); // เคลียร์เศษตัวอักษร

    // แถวที่ 2: LDR และ Degree
    lcd.setCursor(0, 1);
    lcd.print("L:"); lcd.print(ldrValue);
    lcd.setCursor(7, 1);
    lcd.print("Deg:"); lcd.print(currentDeg, 1);
    lcd.print("  "); 

    // ส่ง MQTT
    client.publish(topic_ldr, String(ldrValue));
    client.publish(topic_degree, String(currentDeg, 2));
    
    lastReportTime = millis();
  }

  // --- ตรรกะควบคุมมอเตอร์ ---
  if (!isEmergency) {
    if (ldrValue >= 300 && ldrValue <= 1000) {
      stepper.setSpeed(600); 
      stepper.runSpeed();
    } else {
      stepper.moveTo(0);
      stepper.run();
    }
  }

  // รีเซ็ตรอบเมื่อครบ 360 องศา
  if (abs(stepper.currentPosition()) >= stepsPerRevolution) {
    stepper.setCurrentPosition(0);
  }
}
