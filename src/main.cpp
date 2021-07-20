/* This is the `PapierScherm` firmware, meant to run on the LILYGO® TTGO T5
V2.3.1. Check the back of your PCB to confirm but it's the version with side
buttons. Not bottom buttons.

The firmware assumes you have an MQTT server running that publishes numbers in
the `LineProtocol` format as used by InfluxDB. It will listen to messages and
match all that have a `room=$room` tag, where the list of rooms is defined in
the settings.

It will then show the current room temperature (as set in settings) up top, then
the average temperature, and the outside temperature. The average temperature
does not include the room set as `outside`. */

#include <Arduino.h>
#include <ArduinoOTA.h>

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
    #define ESPMAC (Sprintf("%06" PRIx64, ESP.getEfuseMac() >> 24)).c_str()
#elif ESP8266
    #define ESPMAC (Sprintf("%06" PRIx32, ESP.getChipId())).c_str()
#endif

#define HOST_NAME ROOM_NAME "-" FIRMWARE_NAME

#define every(t) for (static uint32_t _lasttime; (uint32_t)((uint32_t) millis() - _lasttime) >= (t); _lasttime = millis())

#define MAX_DISPLAY_BUFFER_SIZE 800 
#define MAX_HEIGHT(EPD) (EPD::HEIGHT <= MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8) ? EPD::HEIGHT : MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8))


MQTTClient mqtt(256);
GxEPD2_BW<GxEPD2_213_B73, GxEPD2_213_B73::HEIGHT> display(GxEPD2_213_B73(/*CS=5*/ 5, /*DC=*/ 17, /*RST=*/ 16, /*BUSY=*/ 4)); // GDEH0213B73
Button2 btn1(BUTTON_1);

/* Keep track of state for each 'sensor' and then 'room:value' mapping so we
can render from that. */
std::map<String, std::map<String, String>> state;

/* Small helper function if you want to figure out what is being received and
kept track of in the state map. */
void dump_state() {
  for(auto sensor: state) {
      Serial.print(sensor.first + " ");

      for(auto room: sensor.second) {
        Serial.print(room.first + "=" + room.second + " ");
      }

      Serial.println();
  }
}

/* Use the bounding box of some text plus display width to center align text
in that area. */
void draw_center_align(String text, int yy) {
    int16_t x, y;
    uint16_t w, h;

    display.getTextBounds(text, 0, yy, &x, &y, &w, &h);

    display.setCursor((display.width() - w) / 2 - x, yy);
    display.print(text);
}

/* Draw the current state map to the display. */
void draw_state() {
    Serial.println("draw_state: drawing state");

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

/* Say hello to our MQTT server. */
void say_hello() {
    String topic = Sprintf(
        "/debug/hello/%s/%s",
        ROOM_NAME,
        FIRMWARE_NAME
    )

    String payload = Sprintf(
        "room=%s firmware=%s,ip=%s,hostname=%s,mac=%s",
        ROOM_NAME,
        FIRMWARE_NAME,
        WiFi.localIP().toString().c_str(),
        HOST_NAME,
        ESPMAC
    );

    if(!mqtt.publish(topic, payload, true, 0)) {
        Serial.println("say_hello: failed publishing message");
    } else {
        Serial.println("say_hello: said hello");
    }
}

/* Say ping to our MQTT server. */
void say_ping() {
    String topic = Sprintf(,
        "/debug/ping/%s/%s",
        ROOM_NAME,
        FIRMWARE_NAME
    );

    String payload = Sprintf(
        "room=%s firmware=%s,ip=%s,hostname=%s,mac=%s",
        ROOM_NAME,
        FIRMWARE_NAME,
        WiFi.localIP().toString().c_str(),
        HOST_NAME,
        ESPMAC
    );

    if(!mqtt.publish(topic, payload, false, 0)) {
        Serial.println("say_ping: failed publishing message");
    } else {
        Serial.println("say_ping: sent ping");
    }
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
    WiFi.setHostname(HOST_NAME);

    Serial.println(Sprintf("setup_wifi: connecting to %s", WIFI_SSID));

    while (WiFi.status() != WL_CONNECTED) delay(500);
}

void loop_wifi() {
    if ((WiFi.status() != WL_CONNECTED)) {

        Serial.println(
            Sprintf(
                "loop_wifi: lost connection to %s, reconnecting",
                WIFI_SSID
            )
        );

        WiFi.disconnect();
        WiFi.reconnect();

        while (WiFi.status() != WL_CONNECTED) delay(500);
    }
}

/* When a new MQTT message is received it is parsed and then written to the
global state. */
void callback_mqtt(String &topic, String &payload) {
    struct line_protocol message;

    if(strstr("/control/reboot/" ROOM_NAME "/" HOST_NAME, topic.c_str()) != NULL) {
        Serial.println("callback_mqtt: rebooting");
        ESP.restart();
        return;
    }

    if(line_protocol_parse(message, payload)) {
        Serial.println(
            Sprintf(
                "callback_mqtt: received unparseable message on topic %s with data %s",
                topic.c_str(),
                payload.c_str()
            )
        );

        return;
    }

    // Ensure that the line protocol in the message contains the `room` tag and
    // `value` field. 
    if(line_protocol_validate(message, { "room" }, { "value" })) {
        Serial.println(
            Sprintf(
                "callback_mqtt: received unvalidated message on topic %s with data %s",
                topic.c_str(),
                payload.c_str()
            )
        );

        return;
    }

    if(message.measurement != "temperature") return;
    if(strstr(ROOMS, message.tags["room"].c_str()) == NULL) return;

    Serial.println(
        Sprintf(
            "callback_mqtt: message on topic %s with data %s",
            topic.c_str(),
            payload.c_str()
        )
    );

    state[message.measurement][message.tags["room"]] = message.fields["value"];

    // We also update the average in here, where we don't count the 'external-*'
    // room since it is outside.
    if(state.count("temperature")) {
        double total = 0;
        size_t count = 0;

        for(auto room: state["temperature"]) {
            if(String(room.first).startsWith("external")) continue;

            total += String(room.second).toFloat();
            count++;
        }

        state["temperature"]["average"] = total / count;
    }
}

void connect_mqtt() {
    Serial.println(
        Sprintf("connect_mqtt: connecting to %s:%d", MQTT_SERVER, MQTT_PORT)
    );

    while(!mqtt.connect(HOST_NAME)) delay(500);

    mqtt.subscribe("/control/reboot/" ROOM_NAME "/" HOST_NAME);
    mqtt.subscribe("/sensor/temperature");
    mqtt.subscribe("/external/weather-monitoring");

    say_hello();
}


/* Connect to the appropriate MQTT server and setup subscriptions to topics,
note that messages on these topics should be in the InfluxDB Line Protocol
format. */
void setup_mqtt() {
    static WiFiClient wificlient;

    Serial.println(
        Sprintf("setup_mqtt: configuring with %s:%d", MQTT_SERVER, MQTT_PORT)
    );

    mqtt.begin(MQTT_SERVER, MQTT_PORT, wificlient);
    mqtt.onMessage(callback_mqtt);

    connect_mqtt();
}

void loop_mqtt() {
    mqtt.loop();

    if(!mqtt.connected()) {
        Serial.println(
            Sprintf(
                "loop_mqtt: lost connection to %s:%d, reconnecting",
                MQTT_SERVER,
                MQTT_PORT
            )
        );

        connect_mqtt();
    }
}

void setup_button() {
    btn1.setPressedHandler([](Button2 &b) {
        Serial.println("button_callback: btn1 pressed");

        draw_state();
    });
}

void loop_button() {
    btn1.loop();
}

void setup_ota() {
    ArduinoOTA.setHostname(HOST_NAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);
    ArduinoOTA.begin();
}

void loop_ota() {
    ArduinoOTA.handle();
}

/* Call all of our setups and ready the Serial output for use. */
void setup() {
    Serial.begin(115200);

    setup_display();
    setup_wifi();
    setup_mqtt();
    setup_ota();
    setup_button();

    draw_state();
}

void loop() {
    loop_wifi();
    loop_mqtt();
    loop_ota();
    loop_button();

    // Refresh the screen every 3 minutes, epaper clearing has an annoying
    // flashing animation and we don't want to redraw too often.
    every(300 * 1000) draw_state();
    every(5 * 1000) say_ping();
}
