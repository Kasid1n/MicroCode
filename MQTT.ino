#include <WiFi.h>
#include <MQTT.h>

// --- กำหนดขา Pin ---
#define BUTTON_PIN 23  // ขาสำหรับปุ่ม
#define LED_PIN 18      // ขาสำหรับ LED (Built-in)

// --- การตั้งค่า WiFi และ MQTT ---
const char ssid[] = "@JumboPlusIoT";
const char pass[] = "07450748";

const char mqtt_broker[] = "test.mosquitto.org";
const char mqtt_topic[] = "groupAi4/command";
const char mqtt_client_id[] = "AI Group 4"; // เปลี่ยน ID ให้ไม่ซ้ำ
int MQTT_PORT = 1883;

WiFiClient net;
MQTTClient client;

// --- ตัวแปรสำหรับจัดการปุ่ม (Debounce) ---
int lastButtonState = HIGH;  // สถานะปุ่มก่อนหน้า (HIGH = ไม่กด เพราะใช้ INPUT_PULLUP)
int currentButtonState;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;  // เวลาหน่วงกันสัญญาณรบกวน 50ms

// --- ฟังก์ชันเมื่อได้รับการเชื่อมต่อ ---
void connect() {
  Serial.print("checking wifi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }

  Serial.print("\nconnecting mqtt...");
  // เชื่อมต่อ MQTT
  while (!client.connect(mqtt_client_id)) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("\nconnected!");

  // *สำคัญ*: Subscribe เพื่อรอรับคำสั่งเปิด-ปิดไฟ
  client.subscribe(mqtt_topic);
}

// --- ฟังก์ชัน Callback เมื่อมีข้อความเข้า (ส่วนควบคุม LED) ---
void messageReceived(String &topic, String &payload) {
  Serial.println("incoming: " + topic + " - " + payload);

  // ตรวจสอบคำสั่งที่ได้รับ (แปลงเป็นตัวพิมพ์เล็กก่อนเช็ค)
  String command = payload; // สร้างตัวแปรใหม่เพื่อไม่ให้กระทบต้นฉบับถ้าต้องการใช้
  command.toLowerCase(); 

  if (command == "on") {
    digitalWrite(LED_PIN, HIGH);  // เปิด LED
    Serial.println("Action: LED turned ON");
  } 
  else if (command == "off") {
    digitalWrite(LED_PIN, LOW);   // ปิด LED
    Serial.println("Action: LED turned OFF");
  }
  // ถ้าได้รับข้อความที่ตัวเองส่งไป (เช่น Button Pressed) ก็จะเข้าเงื่อนไข else นี้
  else {
    Serial.println("Action: Ignored or Unknown command"); 
  }
}

void setup() {
  Serial.begin(9600);

  // --- Setup Hardware ---
  pinMode(BUTTON_PIN, INPUT_PULLUP); // ตั้งค่าปุ่ม
  pinMode(LED_PIN, OUTPUT);          // ตั้งค่า LED
  digitalWrite(LED_PIN, LOW);        // เริ่มต้นปิดไฟไว้ก่อน

  // --- Setup Network ---
  WiFi.begin(ssid, pass);
  client.begin(mqtt_broker, MQTT_PORT, net);
  client.onMessage(messageReceived); // ผูกฟังก์ชัน callback

  connect();

  Serial.println("System Ready!");
  Serial.println("1. Press Button to Publish status.");
  Serial.println("2. Send 'on'/'off' via MQTT to control LED.");
}

void loop() {
  client.loop();
  delay(10);

  // ตรวจสอบการเชื่อมต่อ ถ้าหลุดให้ต่อใหม่
  if (!client.connected()) {
    connect();
  }

  // --- ส่วนอ่านค่าปุ่ม (Button Logic) ---
  int reading = digitalRead(BUTTON_PIN);

  // เช็คว่าสถานะเปลี่ยนหรือไม่ (เพื่อเริ่มนับเวลา Debounce)
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  // ถ้าเวลาผ่านไปเกิน debounceDelay (คือนิ่งแล้ว)
  if ((millis() - lastDebounceTime) > debounceDelay) {
    
    // ถ้าสถานะเปลี่ยนไปจากสถานะที่จำไว้ล่าสุด (currentButtonState)
    if (reading != currentButtonState) {
      currentButtonState = reading;

      // ทำงานเมื่อสถานะเป็น LOW (กดปุ่ม) หรือ HIGH (ปล่อย)
      if (currentButtonState == LOW) {
        Serial.println(">> Button Pressed: Publishing...");
        client.publish(mqtt_topic, "Button Pressed");
      } else {
        Serial.println(">> Button Released: Publishing...");
        client.publish(mqtt_topic, "Button Released");
      }
    }
  }

  lastButtonState = reading;
}
