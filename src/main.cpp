#include <Arduino.h>

#include <SPI.h>
#include <WiFi.h>
#include <Wire.h>
#include <MQTT.h>

#include <GxEPD2_BW.h>

#include <LineProtocol.h>

#include <map>

#include <Fonts/FreeSansBold9pt7b.h>
#include "FreeSans7pt7b.h"

#include "setting.h"

#define every(t) for (static uint16_t _lasttime; (uint16_t)((uint16_t) millis() - _lasttime) >= (t); _lasttime = millis())

#define MAX_DISPLAY_BUFFER_SIZE 800 
#define MAX_HEIGHT(EPD) (EPD::HEIGHT <= MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8) ? EPD::HEIGHT : MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8))

MQTTClient mqtt;

GxEPD2_BW<GxEPD2_213_B73, GxEPD2_213_B73::HEIGHT> display(GxEPD2_213_B73(/*CS=5*/ 5, /*DC=*/ 17, /*RST=*/ 16, /*BUSY=*/ 4)); // GDEH0213B73

/* Keep track of state for each 'sensor' and then 'room:value' mapping so we
 * can render from that. */
std::map<String, std::map<String, String>> state;

/* Small helper function if you want to figure out what is being received. */
void dump_state() {
  for(auto sensor: state) {
      Serial.print(sensor.first + " ");

      for(auto room: sensor.second) {
        Serial.print(room.first + "=" + room.second + " ");
      }

      Serial.println();
  }
}

/* Draw the current global state to the display. */
void draw_state() {
    display.setFont(&FreeSansBold9pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setTextSize(0.75);
    display.setFullWindow();

    display.firstPage();
    display.fillScreen(GxEPD_WHITE);

    display.setCursor(0, 12);

    display.print("Temperatures");

    display.setFont(&FreeSans7pt7b);

    int i = 30;

    for(auto room: state["temperature"]) {
      display.setCursor(0, i);
      display.print(room.first + " " + String(room.second.toFloat(), 1));
      i += 12;
    }

    display.nextPage();
}

/* Setup the display, rotate it correctly. */
void setup_display() {
    display.init();
    display.setRotation(0);

    display.setFullWindow();
}

/* Connect to WiFi */
void setup_wifi() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) delay(500);
}

void loop_wifi() {
}

/* When a new MQTT message is received it is parsed and then written to the
 * global state. */
void callback_mqtt(String &topic, String &payload) {
    struct line_protocol message;

    if(line_protocol_parse(message, payload)) {
        return;
    }

    state[message.measurement][message.tags["room"]] = message.fields["value"];
}

/* Connect to the appropriate MQTT server and setup subscriptions to topics,
 * note that messages on these topics should be in the InfluxDB Line Protocol
 * format. */

void setup_mqtt() {
    static WiFiClient wificlient;
    mqtt.begin("192.168.1.10", 1883, wificlient);
    mqtt.onMessage(callback_mqtt);

    while(!mqtt.connect("")) delay(500);

    mqtt.subscribe("/sensor/temperature");
    mqtt.subscribe("/esp8266/temperature");
}

void loop_mqtt() {
    mqtt.loop();
}

/* Call all of our setups and ready the Serial output for use. */
void setup() {
    Serial.begin(115200);

    setup_display();
    setup_wifi();
    setup_mqtt();
}

void loop() {
    loop_wifi();
    loop_mqtt();

    // Refresh the screen every 5 minutes, epaper clearing has an annoying
    // flashing animation and we don't want to redraw too often.
    every(300000) draw_state();
};