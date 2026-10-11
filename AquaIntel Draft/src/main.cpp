#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ---------- Pins ----------
const uint8_t PH_PIN       = A0;
const uint8_t TDS_PIN      = A1;
const uint8_t TURB_PIN     = A2;
const uint8_t ONE_WIRE_PIN = 2;   // DS18B20 data (with 4.7k pull-up to 5V)
const uint8_t BUZZER_PIN   = 5;
// LCD I2C: SDA = A4, SCL = A5

// ---------- Settings (adjust these) ----------
const bool  ALERTS_ENABLED = true;   // false = buzzer muted (screen still shows SAFE/UNSAFE)
const bool  VERDICT_FIRST  = true;   // true = SAFE/UNSAFE is the first screen
const bool  ACTIVE_BUZZER  = true;   // true = sounds when given 5V, false = passive buzzer
const float PH_OFFSET      = 0.0;    // tune with a pH 7 buffer

// ---------- Limits ----------
const float PH_MIN = 6.5, PH_MAX = 8.5;
const float TDS_MAX = 500.0;         // ppm
const float TURB_MAX = 5.0;          // rough scale, see readTurbidity()

// ---------- Timing ----------
const unsigned long READ_MS = 2000;  // read sensors
const unsigned long PAGE_MS = 3000;  // time each screen stays up
const uint8_t SCREENS = 5;           // verdict, pH, TDS, turbidity, temperature

LiquidCrystal_I2C lcd(0x27, 16, 2);  // try 0x3F if the screen stays blank
OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature tempSensor(&oneWire);

float phValue = 7.0, tdsValue = 0, turbidity = 0, tempC = 25.0;
bool alertActive = false;
bool buzzerOn = false;
uint8_t page = 0;
unsigned long lastRead = 0, lastPage = 0;

// Average several samples to reduce noise
float readVoltage(uint8_t pin, uint8_t samples = 20) {
  long sum = 0;
  for (uint8_t i = 0; i < samples; i++) {
    sum += analogRead(pin);
    delay(5);
  }
  return (sum / (float)samples) * 5.0 / 1023.0;
}

float readPH() {
  float v = readVoltage(PH_PIN);
  // Typical module: about 2.5 V at pH 7 and 0.18 V per pH unit
  return 7.0 + ((2.5 - v) / 0.18) + PH_OFFSET;
}

float readTDS(float temperature) {
  float v = readVoltage(TDS_PIN);
  float vc = v / (1.0 + 0.02 * (temperature - 25.0));   // temperature compensation
  float tds = (133.42 * vc * vc * vc - 255.86 * vc * vc + 857.39 * vc) * 0.5;
  return tds < 0 ? 0 : tds;
}

float readTurbidity() {
  float v = readVoltage(TURB_PIN);
  // About 4.2 V in clear water, lower voltage means cloudier.
  // Rough scale, compare against clear water and adjust.
  float val = (4.2 - v) * 7.14;
  return val < 0 ? 0 : val;
}

void readSensors() {
  tempSensor.requestTemperatures();
  float t = tempSensor.getTempCByIndex(0);
  if (t != DEVICE_DISCONNECTED_C && t > -50 && t < 125) tempC = t;

  phValue   = readPH();
  tdsValue  = readTDS(tempC);
  turbidity = readTurbidity();
}

// Short reason if the water quality is bad, or nullptr if it is safe
const char* waterProblem() {
  if (phValue < PH_MIN) return "pH too low";
  if (phValue > PH_MAX) return "pH too high";
  if (tdsValue > TDS_MAX) return "TDS too high";
  if (turbidity > TURB_MAX) return "Too cloudy";
  return nullptr;
}

bool checkAlert() {
  if (!ALERTS_ENABLED) return false;
  return waterProblem() != nullptr;
}

// Print a line padded to 16 characters so no lcd.clear() flicker
void printRow(uint8_t row, const char* text) {
  char buf[17];
  snprintf(buf, sizeof(buf), "%-16s", text);
  lcd.setCursor(0, row);
  lcd.print(buf);
}

void showScreen(uint8_t s) {
  char a[10], line[17];
  switch (s) {
    case 0:  // pH
      printRow(0, "pH");
      dtostrf(phValue, 4, 2, a);
      snprintf(line, sizeof(line), "%s  %s", a,
               phValue < PH_MIN ? "LOW" : (phValue > PH_MAX ? "HIGH" : "OK"));
      printRow(1, line);
      break;
    case 1:  // TDS
      printRow(0, "TDS");
      dtostrf(tdsValue, 1, 0, a);
      snprintf(line, sizeof(line), "%s ppm  %s", a, tdsValue > TDS_MAX ? "HIGH" : "OK");
      printRow(1, line);
      break;
    case 2:  // Turbidity
      printRow(0, "Turbidity");
      dtostrf(turbidity, 1, 1, a);
      snprintf(line, sizeof(line), "%s  %s", a, turbidity > TURB_MAX ? "CLOUDY" : "CLEAR");
      printRow(1, line);
      break;
    case 3:  // Temperature
      printRow(0, "Temperature");
      dtostrf(tempC, 1, 1, a);
      snprintf(line, sizeof(line), "%s C", a);
      printRow(1, line);
      break;
    case 4: {  // Verdict
      const char* p = waterProblem();
      if (p == nullptr) {
        printRow(0, "Water quality");
        printRow(1, "SAFE");
      } else {
        printRow(0, p);
        printRow(1, "UNSAFE");
      }
      break;
    }
  }
}

void showPage() {
  // page 0..4 in time order; shift so the verdict screen comes first if wanted
  uint8_t s = VERDICT_FIRST ? (page + SCREENS - 1) % SCREENS : page;
  showScreen(s);
}

void printSerial() {
  const char* p = waterProblem();
  Serial.print("pH=");    Serial.print(phValue, 2);
  Serial.print(" TDS=");  Serial.print(tdsValue, 0);
  Serial.print(" Turb="); Serial.print(turbidity, 1);
  Serial.print(" Temp="); Serial.print(tempC, 1);
  Serial.print(" ");      Serial.println(p == nullptr ? "SAFE" : "UNSAFE");
}

// Buzzer sounds without pause while the water is UNSAFE, and stops when it is SAFE again
void handleAlert() {
  if (alertActive && !buzzerOn) {
    if (ACTIVE_BUZZER) digitalWrite(BUZZER_PIN, HIGH);
    else tone(BUZZER_PIN, 2000);
    buzzerOn = true;
  } else if (!alertActive && buzzerOn) {
    if (ACTIVE_BUZZER) digitalWrite(BUZZER_PIN, LOW);
    else noTone(BUZZER_PIN);
    buzzerOn = false;
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  lcd.init();
  lcd.backlight();
  printRow(0, "AquaIntel");
  printRow(1, "Starting...");
  tempSensor.begin();
  delay(2000);
}

void loop() {
  unsigned long now = millis();

  if (now - lastRead >= READ_MS) {
    lastRead = now;
    readSensors();
    alertActive = checkAlert();
    printSerial();
    showPage();            // refresh the numbers on the current screen
  }

  if (now - lastPage >= PAGE_MS) {
    lastPage = now;
    page = (page + 1) % SCREENS;
    showPage();
  }

  handleAlert();
}