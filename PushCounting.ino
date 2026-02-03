//นับจำนวนครั้งที่กดปุ่ม


#define LED 23
#define BUTTON 32

int count = 0;
int lastState = HIGH;
unsigned long lastTime = 0;

void setup() {
  pinMode(LED, OUTPUT);
  pinMode(BUTTON, INPUT_PULLUP);  // ใช้ Pull-up
  Serial.begin(115200);
}

void loop() {
  int state = digitalRead(BUTTON);

  // ตรวจจับ HIGH → LOW + กันเด้ง
  if (lastState == HIGH && state == LOW && (millis() - lastTime) > 30) {
    count++;
    Serial.println(count);

    // กระพริบ LED ตามจำนวนครั้งที่กด
    for (int i = 0; i < count; i++) {
      digitalWrite(LED, HIGH);
      delay(150);
      digitalWrite(LED, LOW);
      delay(150);
    }

    lastTime = millis();
  }

  lastState = state;
}
