#include <Arduino.h>

#include <SPI.h>
#include <WiFi.h>
#include <Wire.h>
#include <MQTT.h>
#include <Button2.h>

#include <GxEPD2_BW.h>

#include <LineProtocol.h>

#include <map>

#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSans24pt7b.h>

#include "setting.h"

#define Sprintf(f, ...) ({ char* s; asprintf(&s, f, __VA_ARGS__); String r = s; free(s); r; })

#ifdef ESP32
    #define ESPMAC (Sprintf("%06" PRIx64, ESP.getEfuseMac() >> 24))
#elif ESP8266
    #define ESPMAC (Sprintf("%06" PRIx32, ESP.getChipId()))
#endif

#define every(t) for (static uint32_t _lasttime; (uint32_t)((uint32_t) millis() - _lasttime) >= (t); _lasttime = millis())

#define MAX_DISPLAY_BUFFER_SIZE 800 
#define MAX_HEIGHT(EPD) (EPD::HEIGHT <= MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8) ? EPD::HEIGHT : MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8))

MQTTClient mqtt;
GxEPD2_BW<GxEPD2_213_B73, GxEPD2_213_B73::HEIGHT> display(GxEPD2_213_B73(/*CS=5*/ 5, /*DC=*/ 17, /*RST=*/ 16, /*BUSY=*/ 4)); // GDEH0213B73
Button2 btn1(BUTTON_1);

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

void draw_center_align(String text, int yy) {
    int16_t x, y;
    uint16_t w, h;

    display.getTextBounds(text, 0, yy, &x, &y, &w, &h);

    display.setCursor((display.width() - w) / 2 - x, yy);
    display.print(text);
}

/* Draw the current global state to the display. */
void draw_state() {
    display.setTextColor(GxEPD_BLACK);
    display.setTextSize(0.75);
    display.setFullWindow();

    display.firstPage();
    display.fillScreen(GxEPD_WHITE);

    display.setFont(&FreeSansBold9pt7b);
    draw_center_align("Inside", 27);

    display.setFont(&FreeSans24pt7b);
    if(!state.count("temperature") || !state["temperature"].count(ROOM_NAME)) {
        draw_center_align("-", 75);
    } else {
        draw_center_align(String(state["temperature"][ROOM_NAME].toFloat(), 1), 75);
    }

    display.setFont(&FreeSansBold9pt7b);
    draw_center_align("Average", 105);

    display.setFont(&FreeSans24pt7b);
    if(!state.count("temperature") || !state["temperature"].count("average")) {
        draw_center_align("-", 153);
    } else {
        draw_center_align(String(state["temperature"]["average"].toFloat(), 1), 153);
    }

    display.setFont(&FreeSansBold9pt7b);
    draw_center_align("Outside", 183);

    display.setFont(&FreeSans24pt7b);
    if(!state.count("temperature") || !state["temperature"].count(OUTSIDE_NAME)) {
        draw_center_align("-", 231);
    } else {
        draw_center_align(String(state["temperature"][OUTSIDE_NAME].toFloat(), 1), 231);
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
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    WiFi.setHostname((String(ROOM_NAME) + String("-") + String(FIRMWARE_NAME)).c_str());

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

    if(line_protocol_validate(message, { "room" }, { "value" })) {
        return;
    }

    state[message.measurement][message.tags["room"]] = message.fields["value"];

    // We also update the average in here, where we don't count the 'external-*'
    // room since it is outside.
    if(state.count("temperature")) {
        double total = 0;
        size_t count = 0;

        for(auto room: state["temperature"]) {
            if(room.first == "external-6215") continue;

            total += String(room.second).toFloat();
            count++;
        }

        state["temperature"]["average"] = total / count;
    }
}

/* Connect to the appropriate MQTT server and setup subscriptions to topics,
 * note that messages on these topics should be in the InfluxDB Line Protocol
 * format. */

void setup_mqtt() {
    static WiFiClient wificlient;
    mqtt.begin("192.168.1.10", 1883, wificlient);
    mqtt.onMessage(callback_mqtt);

    while(!mqtt.connect("")) delay(500);

    String hostname = WiFi.getHostname();
    String ip = WiFi.localIP().toString();

    String subject = String("/debug/hello");
    String data = String("room=") + String(ROOM_NAME) + String(" ") + String("firmware=") + String("PaperScreen") + String(",ip=") + ip + String(",hostname=") + String(hostname) + String(",mac=") + String(ESPMAC);

    mqtt.publish(subject, data, true, 1);

    mqtt.subscribe("/sensor/temperature");
    mqtt.subscribe("/esp8266/temperature");
    mqtt.subscribe("/external/weather-monitoring");
    mqtt.subscribe("/external/time-monitoring/hhmm");
}

void loop_mqtt() {
    mqtt.loop();
}

void setup_button() {
    btn1.setPressedHandler([](Button2 &b) {
        draw_state();
    });
}

void loop_button() {
    btn1.loop();
}

/* Call all of our setups and ready the Serial output for use. */
void setup() {
    Serial.begin(115200);

    setup_display();
    setup_wifi();
    setup_mqtt();
    setup_button();

    draw_state();
}

void loop() {
    loop_wifi();
    loop_mqtt();
    loop_button();

    // Refresh the screen every 3 minutes, epaper clearing has an annoying
    // flashing animation and we don't want to redraw too often.
    every(300 * 1000) draw_state();
};
