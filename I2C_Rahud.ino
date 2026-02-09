#include <Wire.h>
#include <LCD_I2C.h>

#define DS1307_ADDRESS 0x68  // I2C Address ของ DS1307
LCD_I2C lcd(0x27, 16, 2);

// ฟังก์ชันแปลง BCD เป็น Decimal (เพราะชิป RTC เก็บข้อมูลแบบ BCD)
byte bcdToDec(byte val) {
  return ((val / 16 * 10) + (val % 16));
}

void setup() {
  Wire.begin();        // เริ่มการสื่อสาร I2C
  Serial.begin(9600);
  
  lcd.begin();
  lcd.backlight();
  
  // ตรวจสอบเบื้องต้นว่ามีอุปกรณ์ตอบสนองไหม
  Wire.beginTransmission(DS1307_ADDRESS);
  if (Wire.endTransmission() != 0) {
    lcd.print("RTC not found!");
    while(1);
  }
}
const char* days[] = {"", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

void loop() {
  // 1. ส่งคำสั่งไปยัง DS1307 เพื่อบอกว่าจะอ่านตั้งแต่ Register 0x00 (วินาที)
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.write(0x00); 
  Wire.endTransmission();

  // 2. ขออ่านข้อมูล 7 Bytes (วินาที, นาที, ชั่วโมง, วันในสัปดาห์, วันที่, เดือน, ปี)
  Wire.requestFrom(DS1307_ADDRESS, 7);

  if (Wire.available() >= 7) {
    // อ่านค่าและแปลงจาก BCD เป็นเลขฐานสิบ
    byte second = bcdToDec(Wire.read() & 0x7F); // bit 7 คือ CH bit (Clock Halt)
    byte minute = bcdToDec(Wire.read());
    byte hour   = bcdToDec(Wire.read() & 0x3F); // bit 6 คือ 12/24 hour mode
    byte dow    = bcdToDec(Wire.read());        // Day of week
    byte day    = bcdToDec(Wire.read());
    byte month  = bcdToDec(Wire.read());
    int  year   = bcdToDec(Wire.read()) + 2000;

    // 3. แสดงผลบน LCD
    lcd.setCursor(0, 0);
    printDigits(day);   lcd.print("/");
    printDigits(month); lcd.print("/");
    lcd.print(year);
    lcd.print(" ");
    lcd.print(days[dow]); // แสดง Sun, Mon, ...

    lcd.setCursor(4, 1);
    printDigits(hour);   lcd.print(":");
    printDigits(minute); lcd.print(":");
    printDigits(second);
  }

  delay(1000);
}

// ฟังก์ชันช่วยจัดการเลข 0 นำหน้าบน LCD
void printDigits(byte digits) {
  if (digits < 10) lcd.print('0');
  lcd.print(digits);
}
