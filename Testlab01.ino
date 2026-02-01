#include <WiFi.h>
#include <MQTT.h>

// --- กำหนดขา Pin ---
#define BUTTON_PIN 19 
#define MOTOR_PIN 23  
#define LDR_PIN 34    // ขา Analog สำหรับเซ็นเซอร์แสง

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
const char topic_command[] = "groupAi4/command"; // รับคำสั่งจาก Dashboard

WiFiClient net;
MQTTClient client;

// --- ตัวแปรสถานะ ---
bool isAutoMode = true;     // เริ่มต้นที่ Auto Mode
unsigned long lastInterruptTime = 0;
bool motorState = false;    // false = หยุด, true = ทำงาน
int ldrValue = 0;

void IRAM_ATTR handleButtonPress() {
  unsigned long interruptTime = millis();
  // Debounce 250ms ป้องกันสัญญาณรบกวน
  if (interruptTime - lastInterruptTime > 250) {
    isAutoMode = !isAutoMode;
    lastInterruptTime = interruptTime;
  }
}

// Debounce สำหรับปุ่มกดบนบอร์ด
int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

unsigned long lastPublishTime = 0;

void connect() {
  Serial.print("Connecting WiFi...");
  while (WiFi.status() != WL_CONNECTED) { delay(500); }
  
  Serial.print("\nConnecting MQTT...");
  while (!client.connect(mqtt_client_id)) { delay(500); }
  
  Serial.println("\nConnected!");
  client.subscribe(topic_command);
  
  // ส่งสถานะเริ่มต้นไปยัง Dashboard
  publishStatus();
}

void messageReceived(String &topic, String &payload) {
  Serial.println("Message: " + payload);
  
  if (payload == "toggleMode") {
    isAutoMode = !isAutoMode;
  } 
  else if (!isAutoMode) { // ถ้าอยู่ใน Manual Mode เท่านั้นถึงจะรับคำสั่ง On/Off
    if (payload == "motorOn") {
      motorState = !motorState;
    }
  }
  
  publishStatus();
}

void publishStatus() {
  client.publish(topic_mode_status, isAutoMode ? "Auto" : "Manual");
  client.publish(topic_motor_status, motorState ? "ทำงาน" : "หยุด");
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonPress, FALLING);
  
  pinMode(MOTOR_PIN, OUTPUT);
  digitalWrite(MOTOR_PIN, LOW);

  WiFi.begin(ssid, pass);
  client.begin(mqtt_broker, MQTT_PORT, net);
  client.onMessage(messageReceived);

  connect();
}

void loop() {
  client.loop();
  if (!client.connected()) connect();

  // 1. อ่านค่า LDR และส่งขึ้น Dashboard ทุก 1 วินาที
  if (millis() - lastPublishTime > 1000) {
    ldrValue = analogRead(LDR_PIN);
    client.publish(topic_ldr, String(ldrValue));
    lastPublishTime = millis();
    
    // อัปเดตสถานะมอเตอร์ใน Dashboard เผื่อมีการเปลี่ยนแปลงจากเซ็นเซอร์
    client.publish(topic_motor_status, motorState ? "ทำงาน" : "หยุด");
  }

  // 2. ตรรกะการสลับโหมดด้วยปุ่มกดบนบอร์ด (Physical Button)
  int reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonState) lastDebounceTime = millis();
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading == LOW && lastButtonState == HIGH) { // กดปุ่ม
      isAutoMode = !isAutoMode;
      Serial.println(isAutoMode ? "Mode: Auto" : "Mode: Manual");
      publishStatus();
    }
  }
  lastButtonState = reading;

  // 3. ควบคุมมอเตอร์ตามโหมด
  if (isAutoMode) {
    // โหมดออโต้: ใช้ค่า LDR (ทำงานเมื่อ > 700)
    if (ldrValue > 700) motorState = true;
    else motorState = false;
  }

  // สั่งงาน Motor ตามค่า motorState ที่สรุปมาแล้ว
  digitalWrite(MOTOR_PIN, motorState ? HIGH : LOW);
  static unsigned long lastReport = 0;
  if (millis() - lastReport > 1000) {
    Serial.print("Mode: "); Serial.print(isAutoMode ? "AUTO" : "MANUAL");
    Serial.print(" | LDR: "); Serial.print(ldrValue);
    Serial.print(" | Motor: "); Serial.println(motorState ? "ON" : "OFF");

    if (client.connected()) {
      client.publish("groupAi4/ldr", String(ldrValue));
      client.publish("groupAi4/modeStatus", isAutoMode ? "Auto" : "Manual");
      client.publish("groupAi4/motorStatus", motorState ? "ทำงาน" : "หยุด");
    }
    lastReport = millis();
  }
}
