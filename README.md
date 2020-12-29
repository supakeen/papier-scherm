# BellTFT

The `BellTFT` firmware is meant for the LilyGo TTGo T-Display ESP32 module.

## Wormouth

This repository and firmware is part of the [Wormouth](https://supakeen.com/project/wormouth)
project. You can find where the `BellTFT` sources its data and what you need to set up to
send information to the `BellTFT` there.

## Requirements

### Hardware

- LilyGo TTGo T-Display ESP32

### Software

- An MQTT server to connect to.

## Installation

`BellTFT` follows the general Wormouth Installation for firmware. This means that you should
have PlatformIO installed and ready to use.

1. Copy the `env/EXAMPLE` file to an `env/test.ini` file. Fill in your WiFi SSID and password.
2. Connect your device via USB and use the following commands to quickly get it flashed.

```bash
pio run -e test-serial -t upload
pio device monitor
```

## Usage

After booting the device shortly shows some fast text messages about connecting to WiFi and
your MQTT server, it then dims its backlight. The top button next to the screen toggles the
backlight on and off.

The bottom button clears alarms.
