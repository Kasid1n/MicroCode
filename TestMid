#include <WiFi.h>
#include <MQTT.h>

// --- Pins ---
#define MOTOR_ENABLE 23
#define MOTOR_INPUT1 22
#define MOTOR_INPUT2 21
#define LED_SINGLE   4   
#define BUTTON_PIN   18 

// --- Topics ---
const char mqtt_topic[]    = "groupAi4/command";
const char status_motor[]  = "groupAi4/status/motor";
const char status_led[]    = "groupAi4/status/led"; // Topic สำหรับสถานะไฟบน Dashboard

WiFiClient net;
MQTTClient client;

volatile int currentState = 0;    
int memoryDirection = 1;          
volatile bool buttonPressed = false;
unsigned long lastInterruptTime = 0;

void IRAM_ATTR handleButtonInterrupt() {
  unsigned long interruptTime = millis();
  if (interruptTime - lastInterruptTime > 250) {
    currentState++;
    if (currentState > 2) currentState = 0;
    buttonPressed = true;
    lastInterruptTime = interruptTime;
  }
}

void updateMotorState() {
  if (currentState == 0) { // --- สถานะหยุด ---
    digitalWrite(MOTOR_INPUT1, LOW);
    digitalWrite(MOTOR_INPUT2, LOW);
    digitalWrite(LED_SINGLE, LOW);
    
    client.publish(status_motor, "STOPPED");
    client.publish(status_led, "OFF"); // ส่ง OFF เมื่อหยุด
    Serial.println("Motor: STOPPED, LED: OFF");
  } 
  else { // --- สถานะหมุน (1 หรือ 2) ---
    digitalWrite(LED_SINGLE, HIGH);
    client.publish(status_led, "ON"); // ส่ง ON เมื่อมีการหมุน
    
    if (currentState == 1) {
      digitalWrite(MOTOR_INPUT1, HIGH);
      digitalWrite(MOTOR_INPUT2, LOW);
      memoryDirection = 1;
      client.publish(status_motor, "CW");
      Serial.println("Motor: CW, LED: ON");
    } else {
      digitalWrite(MOTOR_INPUT1, LOW);
      digitalWrite(MOTOR_INPUT2, HIGH);
      memoryDirection = 2;
      client.publish(status_motor, "CCW");
      Serial.println("Motor: CCW, LED: ON");
    }
  }
}

// ... (ส่วน connect, messageReceived, setup, loop เหมือนเดิม) ...

void connect() {
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_SINGLE, !digitalRead(LED_SINGLE));
    delay(100);
  }
  while (!client.connect("AI_Group_4_ESP32")) { delay(1000); }
  client.subscribe(mqtt_topic);
  digitalWrite(LED_SINGLE, LOW); 
}

void messageReceived(String &topic, String &payload) {
  payload.toLowerCase();
  if (payload == "on") currentState = memoryDirection; 
  else if (payload == "off" || payload == "stop") currentState = 0;
  else if (payload == "cw") currentState = 1;
  else if (payload == "ccw") currentState = 2;
  updateMotorState();
}

void setup() {
  Serial.begin(115200);
  pinMode(MOTOR_ENABLE, OUTPUT);
  pinMode(MOTOR_INPUT1, OUTPUT);
  pinMode(MOTOR_INPUT2, OUTPUT);
  pinMode(LED_SINGLE, OUTPUT);
  digitalWrite(MOTOR_ENABLE, HIGH);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonInterrupt, FALLING);

  WiFi.begin("@JumboPlusIoT", "07450748");
  client.begin("test.mosquitto.org", net);
  client.onMessage(messageReceived);
  connect();
  updateMotorState();
}

void loop() {
  client.loop();
  if (!client.connected()) connect();
  if (buttonPressed) {
    updateMotorState();
    buttonPressed = false;
  }
}
