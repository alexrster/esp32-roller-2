#ifndef __APP_H
#define __APP_H

#include <Arduino.h>
#include "version.h"

#ifndef INT_LED_PIN
#define INT_LED_PIN                   15
#endif

#define APP_INIT_DELAY_MILLIS         2500

#define WIFI_RECONNECT_MILLIS         10000
#define WIFI_WATCHDOG_MILLIS          60000

#ifndef WIFI_HOSTNAME
#define WIFI_HOSTNAME                 "roller-02"
#endif

#define MQTT_SERVER_PORT              1883
#define MQTT_RECONNECT_MILLIS         5000
#define MQTT_QUEUE_MAX_SIZE           100

#ifndef MQTT_CLIENT_ID
#define MQTT_CLIENT_ID                WIFI_HOSTNAME
#endif

#define MQTT_PATH_PREFIX              "dev/" MQTT_CLIENT_ID
#define MQTT_STATUS_TOPIC             MQTT_PATH_PREFIX "/status"
#define MQTT_VERSION_TOPIC            MQTT_PATH_PREFIX "/version"
// #define MQTT_CONFIG_TOPIC             MQTT_PATH_PREFIX "/config"

#define MQTT_STATUS_ONLINE_MSG        "online"
#define MQTT_STATUS_OFFLINE_MSG       "offline"

#define OTA_UPDATE_TIMEOUT_MILLIS     5*60000

#define WDT_TIMEOUT_SEC               20

#define RELAY_AUDIO_PIN               17
#define RELAY_BLINDS_UP_PIN           16
#define RELAY_BLINDS_DOWN_PIN         18
#define REEDSWITCH_1_PIN              21
#define BUTTON_1_PIN                  33
// #define BUTTON_1_LED_PIN              34
#define BUTTON_1_LED_PIN              INT_LED_PIN

#define BLINDS_ROLLING_TIMELIMIT_MS   60000

extern bool parseBooleanMessage(byte* payload, unsigned int length, boolean defaultValue = false);

#endif