#include <WiFi.h>
#include <time.h>
#include <LCD_I2C.h>
#include <MQTT.h>
#include <AccelStepper.h>

const int stepsPerRevolution = 2048; 
const int motorSpeedRPM = 15; 
const long stepsLimit = 7500;
// ลำดับพินสำหรับ ULN2003: IN1, IN3, IN2, IN4
AccelStepper stepper(AccelStepper::FULL4WIRE, 19, 17, 18, 16); 
LCD_I2C lcd(0x27, 16, 2); 

#define LDR_PIN 34 
const int BUZZER_PIN = 25;

const char ssid[] = "@JumboPlusIoT";
const char pass[] = "07450748";
const char mqtt_broker[] = "test.mosquitto.org";
const char mqtt_client_id[] = "AI_Group4_Control";

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7 * 3600; 

int alarmHour = 12;
int alarmMinute = 53;
bool isOpened = false; 
bool isAutoMode = true;
int ldrValue = 0;

const char topic_set_alarm[] = "groupAi4/setAlarm";
const char topic_pos[] = "groupAi4/pos";

unsigned long lastUpdate = 0;
unsigned long buzzerStartTime = 0;
bool buzzerActive = false;

WiFiClient net;
MQTTClient client;

void connect() {
  Serial.println("Connecting WiFi...");
  if (WiFi.status() != WL_CONNECTED) { return; } // ไม่ใช้ while เพื่อไม่ให้ block มอเตอร์
  
  if (!client.connected()) {
    Serial.println("Connecting MQTT...");
    if (client.connect(mqtt_client_id)) {
      client.subscribe("groupAi4/command");
      client.publish("groupAi4/modeStatus", isAutoMode ? "Auto" : "Manual");
    }
  }
}

void messageReceived(String &topic, String &payload) {
  Serial.println("MQTT Payload: " + payload);
  if (payload == "toggleMode") isAutoMode = !isAutoMode;

  else if (payload == "alarm") {
    digitalWrite(BUZZER_PIN, HIGH); // เปิดเสียง
    buzzerStartTime = millis();    // บันทึกเวลาที่เริ่มร้อง
    buzzerActive = true;           // ตั้งสถานะให้ระบบ Non-blocking ใน loop มาจัดการปิด
    Serial.println("Action: MQTT Buzzer Alarm!");
  }
  else if (!isAutoMode) {
    if (payload == "close") {
      // Logic: ถ้าปัจจุบันไม่ได้อยู่ที่ตำแหน่งปิด (stepsLimit) ให้ไปที่ stepsLimit
      if (stepper.currentPosition() < stepsLimit) {
        stepper.moveTo(stepsLimit);
        Serial.println("Action: Closing Curtain");
      }
      else if (stepper.currentPosition() > 0) {
      // Logic: ถ้าปัจจุบันไม่ได้อยู่ที่ตำแหน่งเปิด (0) ให้ไปที่ 0
        stepper.moveTo(0);
        Serial.println("Action: Opening Curtain");
        
      }
    }
    else if (payload == "half") {
      stepper.moveTo(stepsLimit / 2);
      Serial.println("Action: Move to Half");
    }
  } 
  publishStatus();  
}

void publishStatus() {
  client.publish("groupAi4/modeStatus", isAutoMode ? "Auto" : "Manual");
  client.publish("groupAi4/pos", stepper.currentPosition() > 0 ? "Open" : "Close");
}

void setup() {
  Serial.begin(115200); // ตั้งค่า Serial Monitor ให้ตรงเป็น 115200
  pinMode(BUZZER_PIN, OUTPUT);
  lcd.begin();
  lcd.backlight();

  WiFi.begin(ssid, pass);
  configTime(gmtOffset_sec, 0, ntpServer);

  // ปรับความเร็วและกำลังมอเตอร์ให้ลื่นไหลที่สุด
  float maxSpeedSteps = (motorSpeedRPM * stepsPerRevolution) / 60.0;
  stepper.setMaxSpeed(maxSpeedSteps);
  stepper.setAcceleration(1000.0); // เพิ่มความเร่งเพื่อลดการกระตุก
  stepper.setCurrentPosition(0);
  
  client.begin(mqtt_broker, net);
  client.onMessage(messageReceived);
}

void loop() {
  // 1. เรียกใช้งานมอเตอร์บ่อยที่สุด (หัวใจหลักของความลื่น)
  stepper.run();
  
  // 2. เรียกใช้งาน MQTT
  client.loop();

  // 3. จัดการงานอื่นๆ ทุก 1 วินาที เพื่อไม่ให้กวนจังหวะมอเตอร์
  if (millis() - lastUpdate > 1000) {
    lastUpdate = millis();
    
    // ตรวจสอบการเชื่อมต่อแบบ Non-blocking
    if (WiFi.status() != WL_CONNECTED || !client.connected()) connect();

    // อ่านค่า LDR และส่งข้อมูล
    ldrValue = analogRead(LDR_PIN);
    client.publish("groupAi4/ldr", String(ldrValue));

    // แสดงผล Serial และ LCD
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      Serial.printf("Time: %02d:%02d:%02d | LDR: %d | Pos: %ld\n", 
                    timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, ldrValue, stepper.currentPosition());
      
      lcd.setCursor(0, 0);
      lcd.printf("T:%02d:%02d:%02d %s", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, isAutoMode?"A":"M");
      lcd.setCursor(0, 1);
      lcd.printf("LDR:%4d A:%02d:%02d", ldrValue, alarmHour, alarmMinute);

      // ระบบนาฬิกาปลุก
      if (timeinfo.tm_hour == alarmHour && timeinfo.tm_min == alarmMinute && !isOpened) {
        stepper.moveTo(0);
        digitalWrite(BUZZER_PIN, HIGH);
        buzzerStartTime = millis();
        buzzerActive = true;
        isOpened = true;
      }
    }

    // Auto Mode Logic
    if (isAutoMode) {
      if (ldrValue <= 1700 && stepper.targetPosition() != 0) stepper.moveTo(0);
      else if (ldrValue >= 2300 && stepper.targetPosition() != stepsLimit) stepper.moveTo(stepsLimit);
    }
  }

  // ปิด Buzzer แบบไม่ใช้ delay
  if (buzzerActive && (millis() - buzzerStartTime > 2000)) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerActive = false;
  }
}
