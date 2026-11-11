/*
  NETRA - Standalone GPS test (Phase 4)
  Library needed: "TinyGPSPlus" by Mikal Hart

  Wiring: GPS TX -> ESP32 GPIO16, GPS RX -> ESP32 GPIO17, VCC per module
  silkscreen (3.3-5V), GND to GND.

  NEO-M8N needs open sky to get a fix - a fix INDOORS is unlikely.
  This test will honestly print "NO FIX" until it has real sky view -
  that is expected and correct behavior, not a bug.
*/

#include <TinyGPSPlus.h>
#include "pins.h"

TinyGPSPlus gps;
HardwareSerial GPSSerial(1);

unsigned long lastPrint = 0;

void setup() {
  Serial.begin(115200);
  delay(300);
  GPSSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  Serial.println("[GPS TEST] Started. Waiting for NMEA sentences...");
}

void loop() {
  while (GPSSerial.available() > 0) {
    gps.encode(GPSSerial.read());
  }

  if (millis() - lastPrint > 1000) {
    lastPrint = millis();
    Serial.print("Chars processed: ");
    Serial.print(gps.charsProcessed());
    Serial.print("  Sentences with fix: ");
    Serial.println(gps.sentencesWithFix());

    if (gps.location.isValid() && gps.location.age() < 5000) {
      Serial.print("FIX OK  lat=");
      Serial.print(gps.location.lat(), 6);
      Serial.print(" lon=");
      Serial.print(gps.location.lng(), 6);
      Serial.print(" speed(kmph)=");
      Serial.print(gps.speed.kmph());
      Serial.print(" course=");
      Serial.println(gps.course.deg());
    } else {
      Serial.println("GPS: NO FIX (normal indoors - take it outside/near a window)");
    }

    if (gps.charsProcessed() < 10) {
      Serial.println("WARNING: no NMEA data received at all - check wiring/baud rate (default 9600).");
    }
  }
}
