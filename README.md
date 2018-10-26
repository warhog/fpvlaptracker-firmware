# fpvlaptracker firmware
Firmware of the fpvlaptracker project.

For a general project overview goto https://github.com/warhog/fpvlaptracker

## Folder structure
The legacy folder contains old, unsupported versions just for history. The current code is in fpvlaptracker32.

## Build instructions
The project uses the Arduino software platform for ESP32. I'm using Visual Studio Code with the Arduino extension installed. But you should be able to get it compiled with the Arduino IDE as well (untested).

The **build platform options** are:
* Board: ESP32 Dev Module (esp32)
* PSRAM: disabled
* Partition Scheme: Minimal SPIFFS (Largs Apps with OTA)
* Flash Mode: QIO
* Flash Frequency: 80 MHz
* Flash Size: 4MB (32Mb)
* Upload Speed: 921600
* Core Debug Level: None

**External libraries used:**

|Library|Version|Page|
|-------|-------|----|
|ArduinoJson|5.13.2|http://blog.benoitblanchon.fr/|
