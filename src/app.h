#ifndef __APP_H
#define __APP_H

#include <Arduino.h>
#include "version.h"

#ifndef INT_LED_PIN
#define INT_LED_PIN                   2
#endif

#define BLINDS_ZERO_PIN               2

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

#define SW_RESET_REASON_FILENAME      "/swresr.bin"

#define GMT_OFFSET_SEC                2*60*60
#define DAYLIGHT_OFFSET_SEC           1*60*60

#define NTP_SERVER                    "ua.pool.ntp.org"
#define NTP_UPDATE_MS                 1*60000                                  // every 1 minute

#define OTA_UPDATE_TIMEOUT_MILLIS     5*60000

#define WDT_TIMEOUT_SEC               20
#define UI_FORCED_REDRAW_MS           15*1000

#define RELAY_1_PIN                   17
#define RELAY_2_PIN                   18
#define RELAY_3_PIN                   16
#define REEDSWITCH_1_PIN              21

extern bool parseBooleanMessage(byte* payload, unsigned int length, boolean defaultValue = false);

#endif