#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>

const char* ssid = "Wirelessnet";
const char* password = "BerryWi2023%";
#define VERSION "2.0"  // Change this in the second build to "2.0"
#define LED 2

const char* firmwareUrl = "https://raw.github.com/KeenanKE/ESP32_OTA_Test/blob/main/releases/firmware.bin";

void blinkLED(int delayTime) {
  digitalWrite(LED, HIGH);
  delay(delayTime);
  digitalWrite(LED, LOW);
  delay(delayTime);
}

void checkForUpdates() {
  Serial.println("Checking for updates...");
  HTTPClient http;
  http.begin(firmwareUrl);
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    int contentLength = http.getSize();
    WiFiClient* stream = http.getStreamPtr();
    if (Update.begin(contentLength)) {
      size_t written = Update.writeStream(*stream);
      if (Update.end(true)) {
        Serial.println("Update successful! Rebooting...");
        ESP.restart();
      }
    } else {
      Serial.println("Not enough space for update.");
    }
  } else {
    Serial.println("No update available or connection error.");
  }
  http.end();
}

void setup() {
  Serial.begin(115200);
  pinMode(LED, OUTPUT);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  Serial.println("Connected to WiFi!");
  checkForUpdates();  // Check GitHub once on startup
}

void loop() {
  blinkLED(1000);  // Blink slowly if v1.0, faster after update
}
