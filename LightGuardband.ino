// กำหนดขาของเซนเซอร์แสงและ LED
const int lightSensorPin = A0;   
const int ledPin = 9;            

// *** การตั้งค่า Guard Band (Hysteresis) ***
const int BASE_THRESHOLD = 500;  
const int GUARDBAND = 50;        // ขนาดของ Guard Band (เช่น 50 หน่วย)

// คำนวณ Upper และ Lower Threshold
// Upper Threshold (T_High): 525 
const int UPPER_THRESHOLD = BASE_THRESHOLD + (GUARDBAND / 2); //แก้ตรงนี้เอามิก
// Lower Threshold (T_Low): 475
const int LOWER_THRESHOLD = BASE_THRESHOLD - (GUARDBAND / 2); //แก้ตรงนี้เอามิก

void setup() {
  pinMode(ledPin, OUTPUT);
  Serial.begin(9600);
  Serial.println("Logic Inverted (ON when HIGH) with Anti-Chattering Hysteresis");
  Serial.print("Guard Band Range: ");
  Serial.print(LOWER_THRESHOLD);
  Serial.print(" to ");
  Serial.println(UPPER_THRESHOLD);
}

void loop() {
  int lightValue = analogRead(lightSensorPin);
  // ตรวจสอบสถานะ LED ในปัจจุบัน (HIGH = เปิด, LOW = ปิด)
  int currentLEDState = digitalRead(ledPin); 
  int ledStatus; // สถานะสำหรับ Plotter (0=ปิด, 100=เปิด)

  // *** ตรรกะ Hysteresis ที่แก้ไขแล้ว เพื่อป้องกันไฟกระพริบใน Guard Band ***
  if (currentLEDState == LOW) {
    // 1. ถ้าไฟ 'ปิด' อยู่: รอให้ค่าเซนเซอร์ 'สูงกว่า' Upper Threshold (T_High) ถึงจะเปิด
    if (lightValue > UPPER_THRESHOLD) { 
      digitalWrite(ledPin, HIGH); // เปิดไฟ
      ledStatus = 100;
    } else {
      // อยู่ใน Guard Band หรือต่ำกว่า T_Low: คงสถานะปิด
      ledStatus = 0; 
    }
  } else {
    // 2. ถ้าไฟ 'เปิด' อยู่: รอให้ค่าเซนเซอร์ 'ต่ำกว่า' Lower Threshold (T_Low) ถึงจะปิด
    if (lightValue < LOWER_THRESHOLD) { 
      digitalWrite(ledPin, LOW); // ปิดไฟ
      ledStatus = 0;
    } else {
      // อยู่ใน Guard Band หรือสูงกว่า T_Low: คงสถานะเปิด
      ledStatus = 100; 
    }
  }
  // *** สิ้นสุดการแก้ไขตรรกะ Hysteresis ***

  // ส่งข้อมูลไป Serial Plotter 
  // เส้นที่ 1: ค่าเซนเซอร์แสง (lightValue)
  Serial.print(lightValue);
  Serial.print(",");
  // เส้นที่ 2: ค่า Upper Threshold (UPPER_THRESHOLD)
  Serial.print(UPPER_THRESHOLD);
  Serial.print(",");
  // เส้นที่ 3: ค่า Lower Threshold (LOWER_THRESHOLD)
  Serial.print(LOWER_THRESHOLD);
  Serial.print(",");
  // เส้นที่ 4: สถานะไฟ (0 = ปิด, 100 = เปิด)
  Serial.println(ledStatus);

  delay(100); 
}
