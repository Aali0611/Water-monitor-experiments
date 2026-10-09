#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);  // try 0x3F if the screen stays blank

byte heart[8] = {
  0b00000,
  0b01010,
  0b11111,
  0b11111,
  0b11111,
  0b01110,
  0b00100,
  0b00000
};

void setup() {
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.createChar(0, heart);

  lcd.setCursor(0, 0);
  lcd.print("Hi there! ");
  lcd.write(byte(0));         // heart after the text

  lcd.setCursor(0, 1);
  lcd.print("I'm Aaliyah ");
  lcd.write(byte(0));
}

void loop() {}
