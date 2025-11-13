#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>

const char* ssid = "Wirelessnet";
const char* password = "BerryWi2023%";
#define VERSION "2.0"  // Change this in the second build to "2.0"
#define LED 2

const char* firmwareUrl = "https://github.com/KeenanKE/ESP32_OTA_Test/tree/main/releases";

void blinkLED(int delayTime) {
  digitalWrite(LED, HIGH);
  delay(delayTime);
  digitalWrite(LED, LOW);
  delay(delayTime);
}

/**
 * @brief Check a remote firmware URL and perform an OTA update if a valid image is available.
 *
 * This function performs a blocking HTTP GET to the firmwareUrl, inspects the response
 * size, and streams the payload into the ESP32 Update API. On successful flashing the
 * device is restarted.
 *
 * Behavior:
 * - Prints status and error messages to Serial for debugging.
 * - Uses HTTPClient to perform the GET request and obtains a WiFiClient stream pointer.
 * - Calls Update.begin() with the reported content length, streams the firmware via
 *   Update.writeStream(), then finalizes with Update.end(true).
 * - If Update.end(true) returns true, the function prints a success message and calls
 *   ESP.restart() to reboot into the new firmware.
 * - If Update.begin() fails (e.g. not enough space) or the HTTP GET fails, appropriate
 *   error messages are printed and no restart is attempted.
 * - The HTTPClient is cleaned up by calling http.end() before returning.
 *
 * Preconditions:
 * - WiFi must be connected prior to calling this function.
 * - firmwareUrl must point to a valid HTTP(S) firmware binary accessible by the device.
 * - The calling context must allow blocking I/O (this function blocks until the HTTP
 *   transfer and flashing complete or an error occurs).
 *
 * Notes / Caveats:
 * - If the HTTP response does not include a valid Content-Length (chunked transfer or
 *   unknown size), Update.begin(contentLength) may fail; behavior depends on the
 *   underlying Update implementation.
 * - The function assumes sufficient heap and flash space to hold and write the update;
 *   if not, Update.begin() will fail and an error will be logged.
 * - No retry logic, signature verification, or integrity checking is performed here;
 *   consider adding verification (e.g. hash/signature) if security is required.
 *
 * Side effects:
 * - May restart the device on successful update (calls ESP.restart()).
 * - Writes to Serial for logging.
 *
 * @return void
 */
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
  while (WiFi.status() != WL_CONNECTED) delay(300);
  Serial.println("Connected to WiFi!");
  checkForUpdates();  // Check GitHub once on startup
}

void loop() {
  blinkLED(1000);  // Blink slowly if v1.0, faster after update
}
