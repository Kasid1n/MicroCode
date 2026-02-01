#include <WiFi.h>
#include <MQTT.h>
#include <AccelStepper.h>

// --- กำหนดขา Pin ---
#define LDR_PIN 34 

// --- ตั้งค่า Stepper (28BYJ-48 พร้อม ULN2003) ---
const int stepsPerRevolution = 2048; 
AccelStepper stepper(AccelStepper::FULL4WIRE, 13, 14, 12, 27); 

// --- การตั้งค่า WiFi และ MQTT ---
const char ssid[] = "@JumboPlusIoT";
const char pass[] = "07450748";
const char mqtt_broker[] = "test.mosquitto.org";
const char mqtt_client_id[] = "AI_Group4_Control_V3";
int MQTT_PORT = 1883;

// Topics
const char topic_ldr[] = "groupAi4/ldr";
const char topic_degree[] = "groupAi4/degree";
const char topic_motor_status[] = "groupAi4/motorStatus";
const char topic_command[] = "groupAi4/command"; 

WiFiClient net;
MQTTClient client;

// --- ตัวแปรสถานะ ---
bool isLocked = false; 
int ldrValue = 0;
unsigned long lastReportTime = 0;

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
  Serial.println("Command: " + payload);
  
  if (payload == "lock") {
    isLocked == !isLocked;
  }

  else if (payload == "reset_zero") {
    isLocked = false;
    stepper.moveTo(0);
  }
  publishStatus();
}

void publishStatus() {
  String statusMsg = isLocked ? "Locked (Manual)" : (stepper.distanceToGo() != 0 ? "Moving" : "Static");
  client.publish(topic_motor_status, statusMsg);
}

void setup() {
  Serial.begin(115200);
  stepper.setMaxSpeed(800);
  stepper.setAcceleration(400);
  stepper.setCurrentPosition(0);

  WiFi.begin(ssid, pass);
  client.begin(mqtt_broker, MQTT_PORT, net);
  client.onMessage(messageReceived);

  connect();
}

void loop() {
  client.loop();
  if (!client.connected()) connect();

  // 1. อ่านค่าและรายงานผลทุก 1 วินาที
  if (millis() - lastReportTime > 1000) {
    ldrValue = analogRead(LDR_PIN);
    float currentDeg = (float)stepper.currentPosition() / stepsPerRevolution * 360.0;
    
    client.publish(topic_ldr, String(ldrValue));
    client.publish(topic_degree, String(currentDeg, 2));
    
    Serial.print("Deg: "); Serial.print(currentDeg);
    Serial.print(" | LDR: "); Serial.println(ldrValue);
    
    lastReportTime = millis();
    publishStatus();
  }

  // 2. ตรรกะควบคุมตามแสง (ทำงานเมื่อไม่ได้สั่ง Lock)
  if (!isLocked) {
    if (ldrValue >= 1200 && ldrValue <= 1800) {
      stepper.moveTo(stepsPerRevolution); // ไปที่ 360 องศา
    } else {
      stepper.moveTo(0); // กลับมาที่ 0 องศา
    }
  }

  // 3. รันมอเตอร์
  stepper.run();
}
