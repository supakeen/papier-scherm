# PaperScreen

The `PaperScreen` firmware is meant for the LilyGo T-5 Epaper 2.13" ESP32 module (v2.3).

## Wormouth

This repository and firmware is part of the [Wormouth](https://supakeen.com/project/wormouth)
project. You can find where the `PaperScreen` sources its data and what you need to set up to
send information to the `PaperScreen` there.

## Requirements

### Hardware

- LilyGo T-5 Epaper 2.13" ESP32 module (v2.3).

### Software

- An MQTT server to connect to.

## Installation

`PaperScreen` follows the general Wormouth Installation for firmware. This means that you should
have PlatformIO installed and ready to use.

1. Copy the `env/EXAMPLE` file to an `env/test.ini` file. Fill in your WiFi SSID and password.
2. Connect your device via USB and use the following commands to quickly get it flashed.

```bash
pio run -e test-serial -t upload
pio device monitor
```
