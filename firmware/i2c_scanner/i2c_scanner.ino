/*
  NETRA - I2C address scanner (helper, not part of the main firmware)

  Run this once if the LCD stays blank/no backlight-text after wiring, or if
  Serial Monitor prints "LCD not found at I2C address 0x27" from the main
  firmware. It scans every possible I2C address and reports which ones ACK -
  your LCD backpack will show up as 0x27 or 0x3F almost always.

  Wiring: same as main firmware - SDA=GPIO21, SCL=GPIO22.
  No extra library needed - uses built-in Wire only.
*/

#include <Wire.h>

void setup() {
  Serial.begin(115200);
  delay(300);
  Wire.begin(21, 22);
  Serial.println("[I2C SCANNER] Scanning...");
}

void loop() {
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      Serial.print("Device found at 0x");
      if (addr < 16) Serial.print("0");
      Serial.println(addr, HEX);
      found++;
    }
  }
  if (found == 0) {
    Serial.println("No I2C devices found - check wiring (SDA=GPIO21, SCL=GPIO22) and power.");
  } else {
    Serial.print(found);
    Serial.println(" device(s) found. Update LCD_I2C_ADDR in pins.h to match.");
  }
  Serial.println("---- rescanning in 5s ----");
  delay(5000);
}
