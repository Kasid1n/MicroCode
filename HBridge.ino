// กำหนด Pins สำหรับการควบคุม L293D Motor Driver
const int MOTOR_ENABLE = 23; // ENABLE1 (Pin 1)
const int MOTOR_INPUT1 = 22; // INPUT1 (Pin 2)
const int MOTOR_INPUT2 = 21; // INPUT2 (Pin 7)

// กำหนด Pins สำหรับ LED แสดงสถานะ
const int LED_CW = 16;       // LED 3 (แสดงการหมุนตามเข็มนาฬิกา)
const int LED_CCW = 17;      // LED 4 (แสดงการหมุนทวนเข็มนาฬิกา)

void setup() { //H-Bridge L293D
  // ตั้งค่า Pins สำหรับควบคุม L293D เป็น OUTPUT
  pinMode(MOTOR_ENABLE, OUTPUT);
  pinMode(MOTOR_INPUT1, OUTPUT);
  pinMode(MOTOR_INPUT2, OUTPUT);

  // ตั้งค่า Pins สำหรับ LED เป็น OUTPUT
  pinMode(LED_CW, OUTPUT);
  pinMode(LED_CCW, OUTPUT);
  
  // เปิดใช้งาน L293D Driver (ต้องเป็น HIGH เสมอเพื่อให้มอเตอร์ทำงาน)
  digitalWrite(MOTOR_ENABLE, HIGH);
  
  // ตั้งค่าเริ่มต้น: มอเตอร์หยุดหมุนและ LED ดับ
  stopMotor();
  
  Serial.begin(115200); // ESP32 มักใช้ Baud Rate ที่สูงกว่า
  Serial.println("DC Motor Control with L293D on ESP32");
}

void loop() {
  // 1. หมุนตามเข็มนาฬิกา (Clockwise - CW)
  rotateCW();
  Serial.println("Motor: Clockwise (CW) for 5 seconds");
  delay(5000); // หน่วงเวลา 5 วินาที

  // หยุดมอเตอร์ชั่วขณะ (ทางเลือก: เพื่อความชัดเจนในการเปลี่ยนทิศทาง)
  stopMotor();
  delay(500); // หน่วงเวลา 0.5 วินาที
  
  // 2. หมุนทวนเข็มนาฬิกา (Counter-Clockwise - CCW)
  rotateCCW();
  Serial.println("Motor: Counter-Clockwise (CCW) for 5 seconds");
  delay(5000); // หน่วงเวลา 5 วินาที

  // หยุดมอเตอร์ชั่วขณะก่อนวนซ้ำ
  stopMotor();
  delay(500); // หน่วงเวลา 0.5 วินาที
}

// --- ฟังก์ชันควบคุมมอเตอร์และ LED ---

// ฟังก์ชันสำหรับหมุนตามเข็มนาฬิกา (Forward)
void rotateCW() {
  // ควบคุม L293D: INPUT1 = HIGH, INPUT2 = LOW
  digitalWrite(MOTOR_INPUT1, HIGH);
  digitalWrite(MOTOR_INPUT2, LOW);
  
  // ควบคุม LED: LED_CW (GPIO 16) ติด, LED_CCW (GPIO 17) ดับ
  digitalWrite(LED_CW, HIGH);
  digitalWrite(LED_CCW, LOW);
}

// ฟังก์ชันสำหรับหมุนทวนเข็มนาฬิกา (Reverse)
void rotateCCW() {
  // ควบคุม L293D: INPUT1 = LOW, INPUT2 = HIGH
  digitalWrite(MOTOR_INPUT1, LOW);
  digitalWrite(MOTOR_INPUT2, HIGH);

  // ควบคุม LED: LED_CCW (GPIO 17) ติด, LED_CW (GPIO 16) ดับ
  digitalWrite(LED_CW, LOW);
  digitalWrite(LED_CCW, HIGH);
}

// ฟังก์ชันสำหรับหยุดมอเตอร์ (Brake/Coast - ใช้ Low, Low)
void stopMotor() {
  // ควบคุม L293D: INPUT1 = LOW, INPUT2 = LOW (หยุด)
  digitalWrite(MOTOR_INPUT1, LOW);
  digitalWrite(MOTOR_INPUT2, LOW);
  
  // ควบคุม LED: ดับทั้งคู่
  digitalWrite(LED_CW, LOW);
  digitalWrite(LED_CCW, LOW);
}
