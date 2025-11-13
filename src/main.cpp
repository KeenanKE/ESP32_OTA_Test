#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <Preferences.h>

// Allow build-time override via platformio.ini build_flags (see platformio.ini)
#ifndef SERIAL_BAUD
#define SERIAL_BAUD 9600
#endif

const char* ssid = "Wirelessnet";
const char* password = "BerryWi2023%";
// Version number (v1.0)
#define VERSION "1.0"

// LED pin (onboard LED is usually GPIO 2)
#define LED 2

// URL to check for updates later
const char* firmwareUrl = "https://raw.githubusercontent.com/KeenanKE/ESP32_OTA_Test/main/releases/firmware.bin";

Preferences prefs;

int loadPreferredBaud(int defaultBaud = 9600) {
  prefs.begin("cfg", true);          // namespace "cfg", read-only
  int b = prefs.getInt("baud", 0);   // 0 means not set
  prefs.end();
  if (b <= 0) return defaultBaud;
  return b;
}

void savePreferredBaud(int baud) {
  prefs.begin("cfg", false);         // writeable
  prefs.putInt("baud", baud);
  prefs.end();
}

void checkForUpdates() {
  Serial.println("Checking for firmware update...");

  HTTPClient http;
  http.begin(firmwareUrl);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    int contentLength = http.getSize();
    if (contentLength <= 0) {
      Serial.println("No content or content-length unknown.");
      http.end();
      return;
    }

    WiFiClient *stream = http.getStreamPtr();
    Serial.printf("Update size: %d bytes\n", contentLength);

    if (!Update.begin(contentLength)) {
      Serial.println("Not enough space to begin OTA");
      http.end();
      return;
    }

    size_t written = Update.writeStream(*stream);
    Serial.printf("Written %u/%u bytes\n", (unsigned)written, (unsigned)contentLength);

    if (Update.end()) {
      if (Update.isFinished()) {
        Serial.println("Update successful, restarting...");
        http.end();
        delay(100);
        ESP.restart();
      } else {
        Serial.println("Update not finished? Something went wrong.");
      }
    } else {
      Serial.printf("Update failed. Error #: %d\n", Update.getError());
    }
  } else {
    Serial.printf("HTTP GET failed, code: %d\n", httpCode);
  }

  http.end();
}

// When we get a WiFi IP, wait 30 seconds then perform the OTA check.
// Use an event handler so we don't have to modify setup().
WiFiEventId_t wifiEventId = WiFi.onEvent([](WiFiEvent_t event) {
  if (event == SYSTEM_EVENT_STA_GOT_IP) {
    Serial.println("WiFi connected. Waiting 30 seconds before OTA check...");

    // Create a small task so we don't block the WiFi/event loop
    xTaskCreate(
      [](void*){
        // Initial delay to allow services to settle
        vTaskDelay(pdMS_TO_TICKS(30000)); // 30 seconds

        // After the initial delay, keep checking for updates every 15 seconds.
        // Run forever; the task will remain active for the lifetime of the device.
        for (;;) {
          checkForUpdates();
          vTaskDelay(pdMS_TO_TICKS(30000)); // 30 seconds
        }
      },
      "ota_delay_task",
      8192,
      nullptr,
      1,
      nullptr
    );
  }
});

void blinkLED(int delayTime) {
  digitalWrite(LED, HIGH);
  delay(delayTime);
  digitalWrite(LED, LOW);
  delay(delayTime);
}

void setup() {
  // read persisted baud (falls back to 9600 if not set)
  int runtimeBaud = loadPreferredBaud(9600);

  // Optionally ensure NVS is populated for first run:
  // If no key existed, save the default so future boots are explicit
  // (only necessary if you want to guarantee the key exists)
  // savePreferredBaud(runtimeBaud);

  Serial.begin(runtimeBaud);
  pinMode(LED, OUTPUT);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");
}

void loop() {
  blinkLED(1000);  // Blink every 1 second
}
