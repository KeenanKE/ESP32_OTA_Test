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
// --- End Configuration ---


// --- OTA Task ---
/*
* `ota_task(void *parameter)`: This function runs on a separate task (thread).
* Why: The OTA process, especially HTTPClient, uses a lot of stack memory.
*      Running it in the main loop() can cause a stack overflow and crash the ESP32.
*      By creating a dedicated task with a larger stack size (e.g., 8192 bytes),
*      we isolate the memory-intensive operation and prevent it from crashing the system.
* How:
*   1. It's an infinite loop that contains the update logic.
*   2. `vTaskDelay()` is the FreeRTOS equivalent of `delay()`, but it properly yields
*      CPU time to other tasks instead of halting the processor.
*   3. The core update logic is the same as before, but it's now safely sandboxed.
*/
void ota_task(void *parameter) {
    // The task runs in an infinite loop, checking for updates periodically.
    for (;;) {
        Serial.println("[OTA Task] Checking for new firmware...");

        HTTPClient http;
        http.begin(firmwareUrl);

        int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) {
            int contentLength = http.getSize();
            if (contentLength > 0) {
                Serial.printf("[OTA Task] New firmware available. Size: %d bytes\n", contentLength);

                bool canBegin = Update.begin(contentLength);
                if (canBegin) {
                    Serial.println("[OTA Task] Starting update process...");
                    WiFiClient& stream = http.getStream();
                    size_t written = Update.writeStream(stream);

                    if (written == contentLength) {
                        Serial.println("[OTA Task] Wrote: " + String(written) + " successfully");
                    } else {
                        Serial.println("[OTA Task] Wrote only: " + String(written) + "/" + String(contentLength) + ". Error!");
                    }

                    if (Update.end()) {
                        Serial.println("[OTA Task] Update finished!");
                        if (Update.isFinished()) {
                            Serial.println("[OTA Task] Update successful! Rebooting...");
                            ESP.restart();
                        } else {
                            Serial.println("[OTA Task] Update not finished. Something went wrong.");
                        }
                    } else {
                        Serial.println("[OTA Task] Error occurred: " + String(Update.getError()));
                    }
                } else {
                    Serial.println("[OTA Task] Not enough space to begin OTA");
                }
            } else {
                Serial.println("[OTA Task] Content length is zero, skipping update.");
            }
        } else {
            Serial.printf("[OTA Task] HTTP request failed. Error: %s\n", http.errorToString(httpCode).c_str());
        }
        http.end();

        // Wait for the next update check. vTaskDelay is non-blocking for other tasks.
        vTaskDelay(updateInterval / portTICK_PERIOD_MS);
    }
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

    // --- Create OTA Task ---
    /*
    * `xTaskCreate`: This is a FreeRTOS function to create a new task.
    * Parameters:
    *   1. `ota_task`: The function that the task will run.
    *   2. "OTA_Task": A descriptive name for the task (for debugging).
    *   3. 8192: The stack size in bytes. This is a generous amount for the OTA process.
    *   4. NULL: Parameters to pass to the task (none needed here).
    *   5. 1: The priority of the task (1 is a low priority).
    *   6. NULL: A handle to the task (none needed here).
    */
    xTaskCreate(
        ota_task,
        "OTA_Task",
        8192,
        NULL,
        1,
        NULL
    );
}

void loop() {
    // The main loop is now only responsible for the simple blink logic.
    // The memory-intensive OTA check is running safely on its own core/task.
    digitalWrite(ledPin, HIGH);
    delay(1000);
    digitalWrite(ledPin, LOW);
    delay(1000);
    Serial.println("[Blink] Cycle complete.");
}