#if __has_include(<Arduino.h>)
#include <Arduino.h>
#else
#define HIGH 1
#define LOW 0
#define OUTPUT 1

inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline void delay(unsigned long) {}
#endif

void setup() {
  pinMode(13, OUTPUT);
}

void loop() {
  digitalWrite(13, HIGH);
  delay(1000);
  digitalWrite(13, LOW);
  delay(1000);
}