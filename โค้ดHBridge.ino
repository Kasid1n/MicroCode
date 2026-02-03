#include <WiFi.h>
#include <MQTT.h>

// --- กำหนดขา Pin ---

#define BUTTON_PIN 19 
#define LDR_PIN 34    // ขา Analog สำหรับเซ็นเซอร์แสง
const int MOTOR_ENABLE = 23; // ENABLE1 (Pin 1)
const int MOTOR_INPUT1 = 22; // INPUT1 (Pin 2)
const int MOTOR_INPUT2 = 21; // INPUT2 (Pin 7)
const int LED_CW = 16;       // LED 3 (แสดงการหมุนตามเข็มนาฬิกา)
const int LED_CCW = 17;      // LED 4 (แสดงการหมุนทวนเข็มนาฬิกา)

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
const char topic_ledcw[] = "groupAi4/ledcw";
const char topic_ledccw[] = "groupAi4/ledccw";
WiFiClient net;
MQTTClient client;

// --- ตัวแปรสถานะ ---
bool isAutoMode = true;     // เริ่มต้นที่ Auto Mode
unsigned long lastInterruptTime = 0;
bool motorState = false;    // false = หยุด, true = ทำงาน
bool ledcw = false;
bool ledccw = false;
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
}

void rotateCW() {
  // ควบคุม L293D: INPUT1 = HIGH, INPUT2 = LOW
  digitalWrite(MOTOR_INPUT1, HIGH);
  digitalWrite(MOTOR_INPUT2, LOW);
  
  // ควบคุม LED: LED_CW (GPIO 16) ติด, LED_CCW (GPIO 17) ดับ
  digitalWrite(LED_CW, HIGH);
  ledcw = true;
  digitalWrite(LED_CCW, LOW);
  ledccw = false;
}

// ฟังก์ชันสำหรับหมุนทวนเข็มนาฬิกา (Reverse)
void rotateCCW() {
  // ควบคุม L293D: INPUT1 = LOW, INPUT2 = HIGH
  digitalWrite(MOTOR_INPUT1, LOW);
  
  digitalWrite(MOTOR_INPUT2, HIGH);
  
  // ควบคุม LED: LED_CCW (GPIO 17) ติด, LED_CW (GPIO 16) ดับ
  digitalWrite(LED_CW, LOW);
  ledcw = false;
  digitalWrite(LED_CCW, HIGH);
  ledccw = true;
}
void stopMotor() {
  // ควบคุม L293D: INPUT1 = LOW, INPUT2 = LOW (หยุด)
  digitalWrite(MOTOR_INPUT1, LOW);
  digitalWrite(MOTOR_INPUT2, LOW);
  
  // ควบคุม LED: ดับทั้งคู่
  digitalWrite(LED_CW, LOW);
  digitalWrite(LED_CCW, LOW);
  ledcw = false;
  ledccw = true;
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
    client.publish(topic_ledcw, ledcw ? "On" : "Off");
    client.publish(topic_ledccw, motorState ? "On" : "Off");
  }

  // 2. ตรรกะการสลับโหมดด้วยปุ่มกดบนบอร์ด (Physical Button)
  int reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonState) lastDebounceTime = millis();
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading == LOW && lastButtonState == HIGH) { // กดปุ่ม
      isAutoMode = !isAutoMode;
      Serial.println(isAutoMode ? "Mode: Ready" : "Mode: Stop");
      publishStatus();
    }
  }
  lastButtonState = reading;

  // 3. ควบคุมมอเตอร์ตามโหมด
  if (isAutoMode) {
    // โหมดออโต้: ใช้ค่า LDR (ทำงานเมื่อ > 700)
    if (ldrValue > 500) {rotateCW(); motorState = true;}
    else if (ldrValue>300 && ldrValue<500){ rotateCCW(); motorState = true;}
    else {stopMotor(); motorState = false;}
  }
  
  // สั่งงาน Motor ตามค่า motorState ที่สรุปมาแล้ว
  static unsigned long lastReport = 0;
  if (millis() - lastReport > 1000) {
    Serial.print("Mode: "); Serial.print(isAutoMode ? "Ready" : "Stop");
    Serial.print(" | LDR: "); Serial.print(ldrValue);
    Serial.print(" | Motor: "); Serial.println(motorState ? "ON" : "OFF");

    if (client.connected()) {
      client.publish("groupAi4/ldr", String(ldrValue));
      client.publish("groupAi4/modeStatus", isAutoMode ? "Ready" : "Stop");
      client.publish("groupAi4/motorStatus", motorState ? "ทำงาน" : "หยุด");
    }
    lastReport = millis();
  }

  
}
