# ESP32 Over-The-Air (OTA) Updates: A Complete Guide

## Project Overview

This project implements a robust Over-The-Air (OTA) update system for ESP32 microcontrollers. The ESP32 automatically checks for firmware updates from a GitHub repository every 30 seconds and installs them without requiring a physical USB connection.

## What We Built

- **Automatic Update Checking**: ESP32 checks GitHub for new firmware versions every 30 seconds
- **Version-Based Updates**: Uses a `version.txt` file to avoid unnecessary downloads
- **Non-Blocking Operation**: Updates run on a separate FreeRTOS task to prevent crashes
- **Automated Build System**: PlatformIO automatically generates firmware files and version tracking

## The Journey: Challenges and Solutions

### Challenge 1: Library Confusion

**The Problem**: Initially included `ESPAsyncWebServer` library in the project dependencies, but it wasn't being used anywhere in the code.

**What We Learned**: There are two main approaches to ESP32 OTA:

1. **Web Server Method**: Host a web page on the ESP32 where you manually upload firmware
2. **HTTP Client Method** (What we used): ESP32 acts as a client and downloads firmware from a URL

**Solution**: Removed the unused library since our approach uses the built-in `HTTPClient` library to pull updates from GitHub.

```ini
# Removed unnecessary library
lib_deps = 
    ottowinter/ESPAsyncWebServer-esphome@^3.1.0  # ❌ Not needed
```

---

### Challenge 2: The Silent Crash (Stack Overflow)

**The Problem**: After connecting to Wi-Fi, the ESP32 would just stop responding. No error messages, no blinking LED - just silence. The serial monitor would only show the Wi-Fi connection message, then nothing.

**Root Cause**: The `HTTPClient` library requires a LOT of memory (stack space). When we called it from the main `loop()` function, it exceeded the available memory and caused a stack overflow, crashing the ESP32.

**The "Aha!" Moment**: ESP32 has a dual-core processor and uses FreeRTOS (a real-time operating system) under the hood. We can create separate "tasks" (like threads) with their own dedicated memory!

**Solution**: Created a dedicated FreeRTOS task for OTA updates with 8192 bytes of stack memory:

```cpp
// Create a separate task for OTA with generous stack size
xTaskCreate(
    ota_task,        // Function to run
    "OTA_Task",      // Task name for debugging
    8192,            // Stack size in bytes (this was the key!)
    NULL,            // Parameters
    1,               // Priority
    NULL             // Task handle
);
```

**Why This Works**:

- Main `loop()` runs on one task with limited memory
- OTA operations run on a separate task with plenty of memory
- They can't interfere with each other!

---

### Challenge 3: GitHub's Caching System

**The Problem**: We would upload new firmware to GitHub, but the ESP32 kept downloading the old version. The firmware size remained the same even though we made changes.

**Root Cause**: GitHub uses a Content Delivery Network (CDN) to serve files faster. The CDN caches files, so when you update `firmware.bin`, the CDN might serve the old cached version for several minutes (typically 5-10 minutes).

**Solution**: Added HTTP cache-control headers to force the server to bypass the cache:

```cpp
// These headers tell the server: "Give me the FRESH file, not the cached one!"
http.addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
http.addHeader("Pragma", "no-cache");
http.addHeader("Expires", "0");
```

**Better Solution**: Implemented version checking! Instead of checking firmware size, we check a small `version.txt` file first:

```cpp
// 1. Download the tiny version.txt file (fast!)
String remoteVersion = http.getString();
remoteVersion.trim();

// 2. Only download the large firmware.bin if versions differ
if (!remoteVersion.equals(currentVersion)) {
    performFirmwareUpdate();  // Download the big file
}
```

---

### Challenge 4: OTA Not Persisting After Reboot

**The Problem**: The ESP32 would download new firmware, appear to install it successfully, reboot... and then run the OLD code again!

**Root Cause**: The ESP32's flash memory is divided into "partitions" (like separate hard drive sections). For OTA to work, you need:

- Partition 1 (`app0`): Current firmware
- Partition 2 (`app1`): New firmware downloaded via OTA
- Bootloader: Decides which partition to boot from

The default partition table only has ONE app partition, so there's nowhere to store the update!

**Solution**: Add OTA partition scheme to `platformio.ini`:

```ini
[env:esp32doit-devkit-v1]
board_build.partitions = default_16MB.csv  # or min_spiffs.csv for 4MB boards
```

**Note**: After adding this, you must upload via USB **one more time** to write the new partition table to the flash memory.

---

### Challenge 5: Version String Escaping Issues

**The Problem**: When trying to define the firmware version, we got this cryptic error:

```
error: too many decimal points in number
```

**Root Cause**: The version string in `platformio.ini` needs special escaping because it passes through multiple layers (INI file → preprocessor → C++ code).

**Wrong Way**:

```ini
build_flags = -D FIRMWARE_VERSION="1.0.0"  # ❌ Won't work
```

**Right Way**:

```ini
build_flags = -D FIRMWARE_VERSION=\"1.0.0\"  # ✅ Escaped quotes
```

---

### Challenge 6: The Case of the Missing firmware.bin

**The Problem**: After building, `firmware.elf` would be copied to the `releases` folder, but `firmware.bin` was missing or had an old timestamp.

**Root Cause**: Our Python script was triggered after the `.elf` file was created:

```python
env.AddPostAction(os.path.join("$BUILD_DIR", "${PROGNAME}.elf"), after_build)
```

But here's the build order:

1. Compile → Link → Create `.elf` ✅
2. **Script runs here** (but `.bin` doesn't exist yet!)
3. Convert `.elf` to `.bin`

**Solution**: Change the trigger to wait for the `.bin` file:

```python
# Wait for the .bin file instead - it's the last thing created
env.AddPostAction(os.path.join("$BUILD_DIR", "${PROGNAME}.bin"), after_build)
```

---

### Challenge 7: Escaped Characters in version.txt

**The Problem**: The `version.txt` file contained `\"1.0.0\` instead of just `1.0.0`.

**Root Cause**: The version string from PlatformIO's build environment comes as `'\\"1.0.0\\"'` with escaped backslashes and quotes.

**Solution**: Aggressive string cleaning in the Python script:

```python
# Remove all escape characters and quotes
version = define[1].replace('\\', '').replace('"', '').replace("'", '')
```

---

## Final Architecture

### File Structure

```
ESP32_OTA_Test/
├── platformio.ini              # Project config with version number
├── src/
│   └── main.cpp               # Main ESP32 code
├── scripts/
│   └── copy_firmware.py       # Auto-copies files after build
└── releases/
    ├── firmware.bin           # Binary for OTA updates
    ├── firmware.elf           # Debug symbols
    └── version.txt            # Version tracking (e.g., "1.0.0")
```

### How It Works

1. **Build Time**:

   - You set the version in `platformio.ini`
   - Build the project
   - Python script automatically copies `firmware.bin`, `firmware.elf`, and generates `version.txt`
2. **Runtime on ESP32**:

   - Every 30 seconds, the OTA task wakes up
   - Downloads `version.txt` from GitHub (tiny, ~5 bytes)
   - Compares with its own version
   - If different: Downloads `firmware.bin` (~900KB) and installs it
   - If same: Goes back to sleep, saves bandwidth
3. **Update Process**:

   - Increment version in `platformio.ini`: `1.0.0` → `1.0.1`
   - Make your code changes
   - Build the project
   - Push `firmware.bin` and `version.txt` to GitHub
   - ESP32 detects the change and updates itself!

---

## Key Code Snippets

### Main Configuration (platformio.ini)

```ini
[env:esp32doit-devkit-v1]
platform = espressif32
board = esp32doit-devkit-v1
framework = arduino
monitor_speed = 115200
extra_scripts = post:scripts/copy_firmware.py
build_flags = -D FIRMWARE_VERSION=\"1.0.0\"
```

### OTA Task (main.cpp)

```cpp
void ota_task(void *parameter) {
    for (;;) {
        Serial.println("[OTA Task] Checking for new version...");

        HTTPClient http;
        http.begin(versionUrl);
    
        // Force fresh download, no cache
        http.addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        http.addHeader("Pragma", "no-cache");
        http.addHeader("Expires", "0");

        int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) {
            String remoteVersion = http.getString();
            remoteVersion.trim();
        
            Serial.printf("[OTA Task] Current: %s, Remote: %s\n", 
                         currentVersion, remoteVersion.c_str());

            if (!remoteVersion.equals(currentVersion)) {
                Serial.println("[OTA Task] New version! Updating...");
                http.end();
                performFirmwareUpdate();
            } else {
                Serial.println("[OTA Task] Already up to date.");
            }
        }
        http.end();

        // Wait 30 seconds before checking again
        vTaskDelay(updateInterval / portTICK_PERIOD_MS);
    }
}
```

### Automatic File Copy Script (copy_firmware.py)

```python
def after_build(source, target, env):
    # Extract version from build flags
    for define in env.get("CPPDEFINES", []):
        if isinstance(define, tuple) and define[0] == "FIRMWARE_VERSION":
            version = define[1].replace('\\', '').replace('"', '').replace("'", '')
        
            # Write version to file
            version_file_path = os.path.join(releases_dir, "version.txt")
            with open(version_file_path, "w") as f:
                f.write(version)
```

---

## Lessons Learned

1. **Memory Management is Critical**: Embedded systems have limited resources. Always be mindful of stack usage.
2. **Task Separation is Powerful**: FreeRTOS tasks let you isolate memory-intensive operations.
3. **CDN Caching Can Bite You**: Always consider caching when working with cloud-hosted files.
4. **Version Checking Saves Bandwidth**: Downloading a 5-byte text file is much faster than a 900KB binary.
5. **Build Automation is Worth It**: The Python script saves manual copying and prevents version mismatches.
6. **Debug Output is Your Friend**: Serial prints helped us diagnose every issue.
7. **Escaping is Tricky**: When data passes through multiple parsers (INI → Python → C++), escape characters multiply!

---

## Testing Your OTA Setup

1. **First Upload (USB)**:

   ```bash
   platformio run --target upload
   ```
2. **Watch Serial Monitor**:

   - You should see Wi-Fi connection
   - Blink cycle messages every second
   - OTA check messages every 30 seconds
3. **Make an Update**:

   - Change LED blink timing in `main.cpp`
   - Change version in `platformio.ini`: `"1.0.0"` → `"1.0.1"`
   - Build: `platformio run`
   - Commit and push `releases/firmware.bin` and `releases/version.txt`
4. **Watch the Magic**:

   - Wait up to 30 seconds
   - ESP32 will detect the new version
   - Download and install the update
   - Reboot with new LED timing!

---


## Resources

- **PlatformIO**: https://platformio.org/
- **ESP32 Arduino Core**: https://github.com/espressif/arduino-esp32
- **FreeRTOS Tasks**: https://www.freertos.org/taskandcr.html
- **HTTP Headers**: https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Cache-Control

---

## Future Improvements

- [ ] Add HTTPS support for secure updates
- [ ] Implement rollback mechanism if update fails
- [ ] Add firmware signature verification
- [ ] Create a web dashboard to manage versions
- [ ] Implement staged rollouts (update 10% of devices first)

---

**Happy Coding! 🚀**

*This project demonstrates that even "simple" features like OTA updates involve many subtle challenges. Each problem you solve makes you a better embedded systems developer!*
