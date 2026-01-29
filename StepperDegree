#include <Stepper.h>

// 28BYJ-48 ในโหมด Full Step ใช้ 2048 สเต็ปต่อรอบ
const int stepsPerRevolution = 2048; 

// คำนวณจำนวนสเต็ปสำหรับองศา 
const int stepsDegrees = (2048 * 180) / 360;// (2048*องศา) 

// Pin สำหรับ ULN2003 Driver Board: IN1, IN3, IN2, IN4 
Stepper myStepper(stepsPerRevolution, 23, 19, 22, 18);

void setup() {
  // ตั้งความเร็ว (แนะนำ 5-10 RPM สำหรับมอเตอร์รุ่นนี้)
  myStepper.setSpeed(10); 
  Serial.begin(9600);
  Serial.println("Stepper Motor: x Degrees CW/CCW");
}

void loop() {
  // 1. หมุนตามเข็มนาฬิกา 90 องศา
  Serial.println("หมุนตามเข็ม x องศา");
  myStepper.step(stepsDegrees);
  delay(1000); // พัก 1 วินาที

  // 2. หมุนทวนเข็มนาฬิกา 90 องศา
  Serial.println("หมุนทวนเข็ม x องศา");
  myStepper.step(-stepsDegrees);
  delay(1000); // พัก 1 วินาที
}
