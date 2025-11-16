#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>

// --- Configuration ---
// Replace with your Wi-Fi credentials
const char* ssid = "Wirelessnet";
const char* password = "BerryWi2023%";

// URL for the firmware binary on your GitHub
const char* firmwareUrl = "https://raw.githubusercontent.com/KeenanKE/ESP32_OTA_Test/main/releases/firmware.bin";

// Pin for the built-in LED (usually GPIO 2 on dev kits)
const int ledPin = 2;

// Check for updates every 30 seconds
const unsigned long updateInterval = 30000; 
unsigned long previousMillis = 0;
// --- End Configuration ---


// --- Function Explanations ---
/*
* `performUpdate()`: This function handles the core OTA logic.
* Why: It encapsulates the entire update process, from downloading the firmware to restarting the device.
* How:
*   1. An `HTTPClient` object is created to make a request to your `firmwareUrl`.
*   2. It checks if the server responds with `HTTP_CODE_OK` (200), meaning the file was found.
*   3. `Update.begin()` prepares the ESP32's flash memory to be written with the new firmware. It needs the total size of the incoming file to verify there's enough space.
*   4. `Update.writeStream()` takes the binary data from the HTTP response and writes it directly to the flash partition. This is memory-efficient as it doesn't store the whole file in RAM first.
*   5. `Update.end()` finalizes the update. If successful, it will make the new firmware the active one.
*   6. `ESP.restart()` reboots the device to run the newly updated code.
*/
void performUpdate() {
    Serial.println("[OTA] Checking for new firmware...");

    HTTPClient http;
    http.begin(firmwareUrl);

    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        int contentLength = http.getSize();
        if (contentLength > 0) {
            Serial.printf("[OTA] New firmware available. Size: %d bytes\n", contentLength);

            bool canBegin = Update.begin(contentLength);
            if (canBegin) {
                Serial.println("[OTA] Starting update process...");
                // Get the stream of the HTTP response body
                WiFiClient& stream = http.getStream();
                // Write the stream to the Update library
                size_t written = Update.writeStream(stream);

                if (written == contentLength) {
                    Serial.println("[OTA] Wrote: " + String(written) + " successfully");
                } else {
                    Serial.println("[OTA] Wrote only: " + String(written) + "/" + String(contentLength) + ". Error!");
                }

                if (Update.end()) {
                    Serial.println("[OTA] Update finished!");
                    if (Update.isFinished()) {
                        Serial.println("[OTA] Update successful! Rebooting...");
                        ESP.restart();
                    } else {
                        Serial.println("[OTA] Update not finished. Something went wrong.");
                    }
                } else {
                    Serial.println("[OTA] Error occurred: " + String(Update.getError()));
                }
            } else {
                Serial.println("[OTA] Not enough space to begin OTA");
            }
        } else {
            Serial.println("[OTA] Content length is zero, skipping update.");
        }
    } else {
        Serial.printf("[OTA] HTTP request failed. Error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n[Boot] Starting up...");

    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, LOW);

    // Connect to Wi-Fi
    Serial.print("[WiFi] Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        digitalWrite(ledPin, !digitalRead(ledPin)); // Blink LED while connecting
    }
    Serial.println("\n[WiFi] Connected!");
    Serial.print("[WiFi] IP Address: ");
    Serial.println(WiFi.localIP());

    digitalWrite(ledPin, LOW); // Turn LED off once connected

    // Set the initial time for the first update check
    previousMillis = millis();
}

void loop() {
    // --- Non-blocking Update Check ---
    /*
    * Why: Using `delay()` would halt your entire program. The `millis()` approach allows the
    *      main code (like the blinking LED) to run continuously while periodically checking
    *      if it's time to perform a task.
    * How: It records the time of the last check (`previousMillis`). In each loop, it calculates
    *      the elapsed time. If that time exceeds `updateInterval`, it runs the update check
    *      and resets `previousMillis` to the current time.
    */
    if (millis() - previousMillis >= updateInterval) {
        performUpdate();
        previousMillis = millis(); // Reset the timer
    }

    // Your main application code runs here.
    // For this example, we just blink the LED.
    digitalWrite(ledPin, HIGH);
    delay(100);
    digitalWrite(ledPin, LOW);
    delay(100);
    Serial.println("[Blink] Cycle complete.");
}