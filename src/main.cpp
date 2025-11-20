#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>

// --- Configuration ---

// Wi-Fi credentials are loaded from environment variables via build flags
// See .env.example for setup instructions
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// URL for the firmware binary on your GitHub
const char* firmwareUrl = "https://raw.githubusercontent.com/KeenanKE/ESP32_OTA_Test/main/releases/firmware.bin";
const char* versionUrl = "https://raw.githubusercontent.com/KeenanKE/ESP32_OTA_Test/main/releases/version.txt";

// The version of the current firmware. This is set by a build flag in platformio.ini
const char* currentVersion = FIRMWARE_VERSION;

// Pin for the built-in LED (usually GPIO 2 on dev kits)
const int ledPin = 2;

// Check for updates every 30 seconds
const unsigned long updateInterval = 30000; 
// --- End Configuration ---


// --- Function to Perform Firmware Update ---
/*
* `performFirmwareUpdate()`: This function handles downloading and installing the firmware.
* Why: By separating this from the version check, we only download the large firmware file
*      when we know an update is actually available.
* How: It downloads firmware.bin and writes it to the OTA partition using the Update library.
*/
void performFirmwareUpdate() {
    Serial.println("[OTA Update] Starting firmware download...");
    
    HTTPClient http;
    http.begin(firmwareUrl);

    // Add cache-control headers to ensure we get the latest firmware faster
    http.addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    http.addHeader("Pragma", "no-cache");
    http.addHeader("Expires", "0");

    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      // Get the size of the firmware
        int contentLength = http.getSize();
        if (contentLength > 0) {
            Serial.printf("[OTA Update] Firmware size: %d bytes\n", contentLength);
          // Begin the update process
            bool canBegin = Update.begin(contentLength);
            if (canBegin) {
                Serial.println("[OTA Update] Writing firmware to flash...");
                WiFiClient& stream = http.getStream();
                size_t written = Update.writeStream(stream);
              // Check if the write was successful
                if (written == contentLength) {
                    Serial.println("[OTA Update] Wrote: " + String(written) + " bytes successfully");
                } else {
                    Serial.println("[OTA Update] Wrote only: " + String(written) + "/" + String(contentLength) + " bytes. Error!");
                }
              // Finalize the update
                if (Update.end()) {
                    Serial.println("[OTA Update] Update finished!");
                    if (Update.isFinished()) {
                        Serial.println("[OTA Update] Update successful! Rebooting...");
                        ESP.restart();
                    } else {
                        Serial.println("[OTA Update] Update not finished. Something went wrong.");
                    }
                } else {
                    Serial.println("[OTA Update] Error occurred: " + String(Update.getError()));
                }
            } else {
                Serial.println("[OTA Update] Not enough space to begin OTA");
            }
        } else {
            Serial.println("[OTA Update] Content length is zero, skipping update.");
        }
    } else {
        Serial.printf("[OTA Update] Firmware download failed. Error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
}


// --- OTA Task ---
/*
* `ota_task(void *parameter)`: This function runs on a separate task (thread).
* Why: The OTA process, especially HTTPClient, uses a lot of stack memory.
*      Running it in the main loop() can cause a stack overflow and crash the ESP32.
*      By creating a dedicated task with a larger stack size (e.g., 8192 bytes),
*      we isolate the memory-intensive operation and prevent it from crashing the system.
* How:
*   1. It's an infinite loop that checks for version updates periodically.
*   2. First, it downloads the small version.txt file to check if an update is available.
*   3. Only if a new version is detected does it call performFirmwareUpdate().
*   4. `vTaskDelay()` is the FreeRTOS equivalent of `delay()`, but it properly yields
*      CPU time to other tasks instead of halting the processor.
*/
void ota_task(void *parameter) {
    // The task runs in an infinite loop, checking for updates periodically.
    for (;;) {
        Serial.println("[OTA Task] Checking for new version...");

        HTTPClient http;
        http.begin(versionUrl);
        
        // Add cache-control headers to get the latest version file
        http.addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        http.addHeader("Pragma", "no-cache");
        http.addHeader("Expires", "0");

        int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) {
            String remoteVersion = http.getString();
            remoteVersion.trim(); // Remove any leading/trailing whitespace
            Serial.printf("[OTA Task] Current version: %s, Remote version: %s\n", currentVersion, remoteVersion.c_str());

            // Compare the current version with the remote version
            if (remoteVersion.equals(currentVersion)) {
                Serial.println("[OTA Task] Firmware is up to date.");
            } else {
                Serial.println("[OTA Task] New firmware version available! Starting update...");
                http.end(); // Close the version check connection before starting firmware download
                performFirmwareUpdate();
            }
        } else {
            Serial.printf("[OTA Task] Version check failed. HTTP code: %d, Error: %s\n", httpCode, http.errorToString(httpCode).c_str());
        }
        http.end();

        // Wait for the next update check. vTaskDelay is non-blocking for other tasks.
        vTaskDelay(updateInterval / portTICK_PERIOD_MS);
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n[Boot] Starting up...");

    // Initialize the built-in LED pin
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