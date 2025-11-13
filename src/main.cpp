#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>

const char* ssid = "Wirelessnet";
const char* password = "BerryWi2023%";

// Version number (v1.0)
#define VERSION "1.0"

// LED pin (onboard LED is usually GPIO 2)
#define LED 2

// URL to check for updates later
const char* firmwareUrl = "https://github.com/KeenanKE/ESP32_OTA_Test/tree/main/releases/firmware.bin";

void blinkLED(int delayTime) {
  digitalWrite(LED, HIGH);
  delay(delayTime);
  digitalWrite(LED, LOW);
  delay(delayTime);
}

void setup() {
  Serial.begin(9600);
  pinMode(LED, OUTPUT);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");
}

void loop() {
  blinkLED(1000);  // Blink every 1 second
}
