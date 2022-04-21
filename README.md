# papier-scherm

The `papier-scherm` firmware is meant for the LilyGo T-5 Epaper 2.13" ESP32 module (v2.3).

## Requirements

### Hardware

- LilyGo T-5 Epaper 2.13" ESP32 module (v2.3).

### Software

- An MQTT server to connect to, all messages are expected to be in InfluxDBs
  "LineFormat".

## Installation

`papier-scherm` follows my general setup for firmware. This means that you should
have PlatformIO installed and ready to use.

1. Copy the `env/EXAMPLE` file to an `env/test.ini` file. Fill in your WiFi SSID and password.
2. Connect your device via USB and use the following commands to quickly get it flashed.

```bash
pio run -e test-serial -t upload
pio device monitor
```

## Example

![Example](https://raw.githubusercontent.com/supakeen/papier-scherm/master/example.jpg)

## Name

The words *papier* and *scherm* mean *paper* and *screen* in Dutch. I know, not
a very groundbreaking name.
