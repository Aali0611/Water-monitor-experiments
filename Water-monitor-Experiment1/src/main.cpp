#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);  // try 0x3F if the screen stays blank

void setup() {
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("UNO + VS Code");
  lcd.setCursor(0, 1);
  lcd.print("Lab Ready!");
}

void loop() {
  // Loop code
}