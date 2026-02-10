#include <Wire.h>
#include <LCD_I2C.h>
#include "RTClib.h"

// ตั้งค่าที่อยู่ I2C (ปกติหน้าจอ LCD คือ 0x27 หรือ 0x3F)
LCD_I2C lcd(0x27, 16, 2); 
RTC_DS1307 rtc;

char daysOfTheWeek[7][12] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

void setup() {
  Serial.begin(9600);
  
  // เริ่มต้น LCD
  lcd.begin();
  lcd.backlight();
  lcd.print("System Loading...");

  // เริ่มต้น RTC
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    lcd.clear();
    lcd.print("RTC Error!");
    while (1);
  }

  if (!rtc.isrunning()) {
    Serial.println("RTC is NOT running, setting time...");
    // บรรทัดนี้จะตั้งเวลาให้ตรงกับเวลาที่คอมพิวเตอร์ Compile โค้ด
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  
  lcd.clear();
}

void loop() {
  DateTime now = rtc.now();

  // --- แถวที่ 1: แสดง วันที่/เดือน/ปี ---
  lcd.setCursor(0, 0);
  lcd.print(now.day() < 10 ? "0" : ""); lcd.print(now.day());
  lcd.print("/");
  lcd.print(now.month() < 10 ? "0" : ""); lcd.print(now.month());
  lcd.print("/");
  lcd.print(now.year());
  lcd.print(" ");
  lcd.print(daysOfTheWeek[now.dayOfTheWeek()]);

  // --- แถวที่ 2: แสดง เวลา (ชม:นาที:วินาที) ---
  lcd.setCursor(4, 1); // เลื่อนไปตรงกลางแถว 2
  lcd.print(now.hour() < 10 ? "0" : ""); lcd.print(now.hour());
  lcd.print(":");
  lcd.print(now.minute() < 10 ? "0" : ""); lcd.print(now.minute());
  lcd.print(":");
  lcd.print(now.second() < 10 ? "0" : ""); lcd.print(now.second());

  // ส่งออก Serial Monitor ด้วย (ถ้าต้องการเช็ค)
  Serial.print(now.hour());
  Serial.print(":");
  Serial.println(now.minute());

  delay(1000); // อัปเดตทุก 1 วินาที
}
