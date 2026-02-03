#include <WiFi.h>
#include <MQTT.h>
#include <AccelStepper.h>

// --- กำหนดขา Pin ---
#define LDR_PIN 34 
// ลำดับพินสำหรับ ULN2003 (IN1, IN2, IN3, IN4) 
// หมายเหตุ: บางกรณีอาจต้องสลับเป็น 23, 19, 22, 18 ตามจังหวะของมอเตอร์แต่ละรุ่น
AccelStepper stepper(AccelStepper::FULL4WIRE, 23, 19, 22, 18); 

// --- การตั้งค่า WiFi และ MQTT ---
const char ssid[] = "@JumboPlusIoT";
const char pass[] = "07450748";
const char mqtt_broker[] = "test.mosquitto.org";
const char mqtt_client_id[] = "AI_Group4_Final_System";

// Topics
const char topic_ldr[] = "groupAi4/ldr";
const char topic_degree[] = "groupAi4/degree";
const char topic_motor_status[] = "groupAi4/motorStatus";
const char topic_command[] = "groupAi4/command"; 

WiFiClient net;
MQTTClient client;

// --- ตัวแปรสถานะ ---
const int stepsPerRevolution = 2048; 
bool isEmergency = false; 
int ldrValue = 0;
unsigned long lastReportTime = 0;

// --- ฟังก์ชันเชื่อมต่อ WiFi ---
void connectWiFi() {
  Serial.print("\nConnecting to WiFi...");
  WiFi.begin(ssid, pass);
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500);
    Serial.print(".");
    retry++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected! IP: " + WiFi.localIP().toString());
  }
}

// --- ฟังก์ชันเชื่อมต่อ MQTT ---
void connectMQTT() {
  if (WiFi.status() != WL_CONNECTED) return;
  Serial.print("Connecting to MQTT...");
  while (!client.connect(mqtt_client_id)) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nMQTT Connected!");
  client.subscribe(topic_command);
}

// --- ฟังก์ชันส่งสถานะขึ้น Dashboard ---
void publishStatus() {
  if (!client.connected()) return;
  String statusMsg = isEmergency ? "EMERGENCY (LOCKED)" : (stepper.distanceToGo() != 0 ? "Moving" : "Ready");
  client.publish(topic_motor_status, statusMsg);
}

// --- ฟังก์ชันรับคำสั่งจาก MQTT ---
void messageReceived(String &topic, String &payload) {
  Serial.println("MQTT Message: " + payload);
  
  if (payload == "lock" || payload == "emergency") {
    isEmergency = !isEmergency; 
    
    if (isEmergency) {
      stepper.stop(); // สั่งหยุดทันที
      Serial.println("!!! EMERGENCY ACTIVE !!!");
    } else {
      Serial.println("--- EMERGENCY RELEASED ---");
    }
  }
  else if (payload == "reset_zero") {
    stepper.setCurrentPosition(0);
  }
  
  publishStatus();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // ตั้งค่า Stepper
  stepper.setMaxSpeed(1000);
  stepper.setAcceleration(800);
  stepper.setCurrentPosition(0);

  // เชื่อมต่อ Network
  connectWiFi();
  client.begin(mqtt_broker, 1883, net);
  client.onMessage(messageReceived);
  connectMQTT();
}

void loop() {
  // ระบบ Auto-reconnect
  if (WiFi.status() != WL_CONNECTED) connectWiFi();
  if (WiFi.status() == WL_CONNECTED && !client.connected()) connectMQTT();
  
  client.loop();

  // 1. อ่านค่าและรายงานผลทุก 1 วินาที
  if (millis() - lastReportTime > 1000) {
    ldrValue = analogRead(LDR_PIN);
    
    // คำนวณองศาแบบวนลูป 0-359.99
    float currentDeg = fmod((float)stepper.currentPosition() / stepsPerRevolution * 360.0, 360.0);
    if (currentDeg < 0) currentDeg += 360.0;
    
    if (client.connected()) {
      client.publish(topic_ldr, String(ldrValue));
      client.publish(topic_degree, String(currentDeg, 2));
      publishStatus();
    }
    
    Serial.print("LDR: "); Serial.print(ldrValue);
    Serial.print(" | Deg: "); Serial.print(currentDeg);
    Serial.println(isEmergency ? " [EMERGENCY]" : "");
    
    lastReportTime = millis();
  }

  // 2. ตรรกะควบคุมมอเตอร์
  if (!isEmergency) {
    // เงื่อนไขแสง 300 - 700 ให้หมุนไปเรื่อยๆ
    if (ldrValue >= 300 && ldrValue <= 700) {
      stepper.setSpeed(600); // ความเร็วคงที่ (หมุนต่อเนื่อง)
      stepper.runSpeed();
    } else {
      // อยู่นอกระยะแสงให้ค่อยๆ หยุด
      stepper.stop();
      stepper.run();
    }
  } else {
    // สถานะ Emergency: ต้องหยุดนิ่งสนิท
    stepper.stop();
    stepper.run();
  }

  // 3. จัดการเรื่องรอบ (หมุนครบรอบให้ Reset Step เป็น 0)
  if (abs(stepper.currentPosition()) >= stepsPerRevolution) {
    stepper.setCurrentPosition(0);
  }

  // รันระบบความเร่ง (สำหรับช่วงสั่ง stop)
  if (!isEmergency) {
    stepper.run();
  }
}
