#include <WiFi.h>
#include <MQTT.h>
#include <AccelStepper.h>

// --- กำหนดขา Pin ---
#define LDR_PIN 34 

// --- ตั้งค่า Stepper (28BYJ-48 พร้อม ULN2003) ---
const int stepsPerRevolution = 2048; 
AccelStepper stepper(AccelStepper::FULL4WIRE, 23, 19, 22, 18); 

// --- การตั้งค่า WiFi และ MQTT ---
const char ssid[] = "@JumboPlusIoT";
const char pass[] = "07450748";
const char mqtt_broker[] = "test.mosquitto.org";
const char mqtt_client_id[] = "AI_Group4_Emergency_System";

// Topics
const char topic_ldr[] = "groupAi4/ldr";
const char topic_degree[] = "groupAi4/degree";
const char topic_motor_status[] = "groupAi4/motorStatus";
const char topic_command[] = "groupAi4/command"; 

WiFiClient net;
MQTTClient client;

// --- ตัวแปรสถานะ ---
bool isEmergency = false; // เปลี่ยนชื่อจาก isLocked เป็น isEmergency เพื่อความชัดเจน
int ldrValue = 0;
unsigned long lastReportTime = 0;

void connect() {
  Serial.print("Connecting WiFi...");
  while (WiFi.status() != WL_CONNECTED) { delay(500); }
  while (!client.connect(mqtt_client_id)) { delay(500); }
  client.subscribe(topic_command);
  publishStatus();
}

void messageReceived(String &topic, String &payload) {
  if (payload == "lock" || payload == "emergency") {
    isEmergency = !isEmergency;
    stepper.stop(); // หยุดมอเตอร์ทันทีด้วยความเร่งสูงสุด
  }
  else if (payload == "reset_zero") {
    stepper.setCurrentPosition(0);
    Serial.println("Position Reset to 0");
  }
  publishStatus();
}

void publishStatus() {
  String statusMsg = isEmergency ? "Locking" : (stepper.distanceToGo() != 0 ? "Moving" : "Ready");
  client.publish(topic_motor_status, statusMsg);
}

void setup() {
  Serial.begin(115200);
  stepper.setMaxSpeed(1000);
  stepper.setAcceleration(800);
  stepper.setCurrentPosition(0);

  WiFi.begin(ssid, pass);
  client.begin(mqtt_broker, 1883, net);
  client.onMessage(messageReceived);
  connect();
}

void loop() {
  client.loop();
  if (!client.connected()) connect();

  // 1. อ่านค่าและรายงานผลทุก 1 วินาที
  if (millis() - lastReportTime > 1000) {
    ldrValue = analogRead(LDR_PIN);
    
    // คำนวณองศาปัจจุบัน (0-359.99)
    float currentDeg = fmod((float)stepper.currentPosition() / stepsPerRevolution * 360.0, 360.0);
    if (currentDeg < 0) currentDeg += 360.0; // จัดการกรณีหมุนทวนเข็มแล้วค่าติดลบ
    
    client.publish(topic_ldr, String(ldrValue));
    client.publish(topic_degree, String(currentDeg, 2));
    lastReportTime = millis();
    publishStatus();
  }

  // 2. ตรรกะควบคุมมอเตอร์
  if (!isEmergency) {
    if (ldrValue >= 1200 && ldrValue <= 1800) {
      // สั่งให้หมุนไปข้างหน้าเรื่อยๆ แบบ Non-stop
      stepper.setSpeed(600); 
      stepper.runSpeed();
    } else {
      // ถ้าไม่อยู่ในช่วงแสง ให้หยุดนิ่ง ณ ตำแหน่งนั้น
      stepper.stop();
      stepper.run();
    }
  }

  // 3. จัดการเรื่องรอบ (เมื่อครบรอบ 360 ให้รีเซ็ตค่า Step เป็น 0 เพื่อเริ่มนับองศาใหม่)
  if (abs(stepper.currentPosition()) >= stepsPerRevolution) {
    stepper.setCurrentPosition(0);
  }

  // รันมอเตอร์ในกรณีที่ใช้คำสั่งระยะทาง (เช่น stop หรือ moveTo)
  if (!isEmergency) {
    stepper.run();
  }
}
