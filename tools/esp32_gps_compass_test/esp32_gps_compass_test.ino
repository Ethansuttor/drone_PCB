// ESP32 bench test for the salvaged Hubsan GPS (u-blox M8030) and compass.
// Classic ESP32 DevKit pins. For an ESP32-S3, change the pin numbers below.
//
// Wiring (GPS):     red 5V -> 5V/VIN, white GND -> GND,
//                   black TX -> GPIO16 (ESP RX), blue RX -> GPIO17 (ESP TX)
// Wiring (compass): white 5V -> 5V/VIN (or 3V3, see voltage check), green GND -> GND,
//                   black SDA -> GPIO21, red SCL -> GPIO22
//
// MODE 1 = GPS bridge: raw bytes both ways, for u-center or a serial terminal.
//          Connect u-center to the ESP32 COM port at 115200.
//          If u-center changes the GPS baud, change GPS_BAUD and reflash.
// MODE 2 = test: prints NMEA lines plus compass readings twice a second.

#include <Wire.h>

#define MODE 1

const int GPS_RX_PIN = 16;   // ESP32 receives here (GPS TX, black)
const int GPS_TX_PIN = 17;   // ESP32 sends here (GPS RX, blue)
const long GPS_BAUD = 9600;  // M8030 ROM default; try 38400, 57600, 115200 if garbage
const int SDA_PIN = 21;
const int SCL_PIN = 22;

const uint8_t QMC_ADDR = 0x0D;  // QMC5883L
const uint8_t HMC_ADDR = 0x1E;  // HMC5883L
uint8_t magAddr = 0;

bool i2cPresent(uint8_t a) {
  Wire.beginTransmission(a);
  return Wire.endTransmission() == 0;
}

void writeReg(uint8_t a, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(a);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

bool readRegs(uint8_t a, uint8_t reg, uint8_t *buf, uint8_t n) {
  Wire.beginTransmission(a);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(a, n) != n) return false;
  for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}

void scanI2C() {
  Serial.println("I2C scan:");
  int found = 0;
  for (uint8_t a = 1; a < 127; a++) {
    if (i2cPresent(a)) {
      Serial.printf("  device at 0x%02X\n", a);
      found++;
    }
  }
  if (!found) Serial.println("  nothing found (check SDA/SCL swap, power, pull-ups)");
}

void setupMag() {
  if (i2cPresent(QMC_ADDR)) {
    magAddr = QMC_ADDR;
    writeReg(QMC_ADDR, 0x0B, 0x01);  // SET/RESET period
    writeReg(QMC_ADDR, 0x09, 0x1D);  // continuous, 200 Hz, 8 G, OSR 512
    Serial.println("Compass: QMC5883L at 0x0D");
  } else if (i2cPresent(HMC_ADDR)) {
    magAddr = HMC_ADDR;
    writeReg(HMC_ADDR, 0x00, 0x70);  // 8 sample average, 15 Hz
    writeReg(HMC_ADDR, 0x01, 0xA0);  // gain 4.7 G range
    writeReg(HMC_ADDR, 0x02, 0x00);  // continuous mode
    Serial.println("Compass: HMC5883L at 0x1E");
  } else {
    Serial.println("Compass: no QMC5883L or HMC5883L found");
  }
}

void readMag() {
  uint8_t b[6];
  int16_t x, y, z;
  if (magAddr == QMC_ADDR) {
    if (!readRegs(QMC_ADDR, 0x00, b, 6)) { Serial.println("MAG read failed"); return; }
    x = (int16_t)(b[1] << 8 | b[0]);  // little endian, order X Y Z
    y = (int16_t)(b[3] << 8 | b[2]);
    z = (int16_t)(b[5] << 8 | b[4]);
  } else if (magAddr == HMC_ADDR) {
    if (!readRegs(HMC_ADDR, 0x03, b, 6)) { Serial.println("MAG read failed"); return; }
    x = (int16_t)(b[0] << 8 | b[1]);  // big endian, order X Z Y
    z = (int16_t)(b[2] << 8 | b[3]);
    y = (int16_t)(b[4] << 8 | b[5]);
  } else {
    return;
  }
  // Raw counts, uncalibrated. Heading is only rough until hard iron offsets are removed.
  float heading = atan2((float)y, (float)x) * 180.0 / PI;
  if (heading < 0) heading += 360.0;
  Serial.printf("MAG raw x=%6d y=%6d z=%6d  rough heading %.0f deg\n", x, y, z, heading);
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
#if MODE == 2
  delay(500);
  Wire.begin(SDA_PIN, SCL_PIN, 100000);
  scanI2C();
  setupMag();
  Serial.printf("GPS listening at %ld baud\n", GPS_BAUD);
#endif
}

void loop() {
#if MODE == 1
  while (Serial2.available()) Serial.write(Serial2.read());
  while (Serial.available()) Serial2.write(Serial.read());
#else
  static char line[128];
  static uint8_t len = 0;
  static uint32_t lastMag = 0;
  while (Serial2.available()) {
    char c = Serial2.read();
    if (c == '\n') {
      line[len] = 0;
      Serial.println(line);
      len = 0;
    } else if (c != '\r' && len < sizeof(line) - 1) {
      line[len++] = c;
    }
  }
  if (millis() - lastMag > 500) {
    lastMag = millis();
    readMag();
  }
#endif
}
