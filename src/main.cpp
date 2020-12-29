#include <Arduino.h>
#include <ArduinoOTA.h>

#include <TFT_eSPI.h>
#include <SPI.h>
#include <WiFi.h>
#include <Wire.h>
#include <MQTT.h>
#include <Button2.h>
#include <LineProtocol.h>

#include "setting.h"

MQTTClient mqtt;
TFT_eSPI tft = TFT_eSPI(135, 240); // Invoke custom library
Button2 btn1(BUTTON_1);
Button2 btn2(BUTTON_2);

void setup() {
    Serial.begin(115200);
}

void loop() {
    delay(500);
}
