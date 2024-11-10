# ESP32test
 
## Project Overview

This project is the software portion of an AI voice assistant project, including hardware firmware code and a tester service code.

### Firmware Code
* Located in the `hardware` directory.
* Written in C using ESP-IDF 4.4.8.

### Server Test Code
* Written in Python.
* Saves received audio information as WAV files.

### Hardware
* Available at: https://oshwhub.com/wlmh110/esp32-i2s-tf
* Uses an ESP32 to save I2S audio to NVS or a TF card, and send it to a server.
* Uses an INMP441 for audio acquisition.
* Features an RGB LED to indicate current working status.
