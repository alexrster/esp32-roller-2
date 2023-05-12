#include "app.h"
#include <rom/rtc.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <esp_task_wdt.h>
#include <Preferences.h>
#include <SwitchRelay.h>
#include <BlindsController.h>
#include <pubsub.h>
#include "reset_info.h"

RESET_REASON
  reset_reason[2];

unsigned long 
  now = 0,
  lastWifiOnline = 0,
  lastWifiReconnect = 0,
  lastBlindsRead = 0,
  lastOtaHandle = 0,
  otaUpdateStart = 0,
  runCounter = 0;

bool 
  justStarted = true,
  otaUpdateMode = false,
  lastBlindsZero = false;

char
  sw_reset_reason = 0;

Preferences preferences;
WiFiClient wifiClient;
PubSub pubsub(wifiClient);

BlindsController blindsController = BlindsController::create(RELAY_2_PIN, RELAY_3_PIN, REEDSWITCH_1_PIN);

void restart(char code) {
  preferences.putULong("SW_RESET_UPTIME", millis());
  preferences.putUChar("SW_RESET_REASON", code);
  preferences.end();

  delay(200);
  ESP.restart();
}

bool wifiLoop() {
  if (WiFi.status() != WL_CONNECTED) {
    if (now - lastWifiOnline > WIFI_WATCHDOG_MILLIS) restart(RESET_ON_WIFI_WD_TIMEOUT);
    else if (now - lastWifiReconnect > WIFI_RECONNECT_MILLIS) {
      lastWifiReconnect = now;

      if (WiFi.reconnect()) {
        lastWifiOnline = now;
        return true;
      }
    }

    return false;
  }
  
  lastWifiReconnect = now;
  lastWifiOnline = now;
  return true;
}

void otaStarted() {
  now = millis();
  otaUpdateStart = now;
  otaUpdateMode = true;
}

void otaProgress(unsigned int currentBytes, unsigned int totalBytes) {
  now = millis();

  unsigned int 
    minutes = (now-otaUpdateStart)/1000./60.,
    seconds = (now-otaUpdateStart)/1000. - minutes*60,
    progress = ((float)currentBytes / totalBytes) * 20;

  esp_task_wdt_reset();
}

void otaEnd() {
  restart(RESET_ON_OTA_SUCCESS);
}

void otaError(ota_error_t error) {
  delay(5000);
  restart(RESET_ON_OTA_FAIL);
}

void onPubSubRestart(uint8_t *payload, unsigned int length) { 
  restart(RESET_ON_MQTT_RESET_TOPIC);
}

void onPubSubBlindsPositionSet(uint8_t *payload, unsigned int length) { 
  // if (payload[0] == '1') hallAlert.blink(5000);
}

void onBlindsStateChanged() {
  String stateString;
  switch (blindsController.getState()) {
    case BlindsState::RollingUp:
      stateString = "RollingUp";
      break;
    case BlindsState::RollingDown:
      stateString = "RollingDown";
      break;
    case BlindsState::FullUp:
      stateString = "FullUp";
      break;
    case BlindsState::FullDown:
      stateString = "FullDown";
      break;
    default:
      stateString = "Unknown";
      break;
  }

  pubsub.publish(MQTT_PATH_PREFIX "/blinds/state", stateString.c_str());
}

void setup_pubsub() {
  pubsub.subscribe(MQTT_PATH_PREFIX "/restart", MQTTQOS0, onPubSubRestart);
  pubsub.subscribe(MQTT_PATH_PREFIX "/blinds/position/set", MQTTQOS0, onPubSubBlindsPositionSet);
}

void setup() {
  reset_reason[0] = rtc_get_reset_reason(0);
  reset_reason[1] = rtc_get_reset_reason(1);

  preferences.begin("roller-02");
  runCounter = preferences.getULong("__RUN_N", 0) + 1;
  preferences.putULong("__RUN_N", runCounter);

  pinMode(RELAY_1_PIN, OUTPUT);
  pinMode(RELAY_2_PIN, OUTPUT);
  pinMode(RELAY_3_PIN, OUTPUT);

  sw_reset_reason = preferences.getUChar("SW_RESET_REASON");

  // Setup WDT
  esp_task_wdt_init(WDT_TIMEOUT_SEC, true);
  esp_task_wdt_add(NULL);

  WiFi.setHostname(WIFI_HOSTNAME);
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(wifi_ps_type_t::WIFI_PS_NONE);
  WiFi.begin(WIFI_SSID, WIFI_PASSPHRASE);

  ArduinoOTA.setRebootOnSuccess(true);
  ArduinoOTA.onStart(otaStarted);
  ArduinoOTA.onProgress(otaProgress);
  ArduinoOTA.onEnd(otaEnd);
  ArduinoOTA.onError(otaError);
  ArduinoOTA.begin();

  setup_pubsub();

  blindsController.onBlindsStateChanged(onBlindsStateChanged);

  now = millis();
  lastWifiOnline = now;
}

bool onJustStarted() {
    bool result = true;
    result &= pubsub.publish(MQTT_PATH_PREFIX "/restart_reason/0", get_reset_reason_info(reset_reason[0]).c_str(), true);
    result &= pubsub.publish(MQTT_PATH_PREFIX "/restart_reason/1", get_reset_reason_info(reset_reason[1]).c_str(), true);
    result &= pubsub.publish(MQTT_PATH_PREFIX "/restart_reason/uptime", String(preferences.getULong("SW_RESET_UPTIME", 0)).c_str(), true);
    result &= pubsub.publish(MQTT_PATH_PREFIX "/restart_reason/code", String((uint8_t)sw_reset_reason).c_str(), true);
    result &= pubsub.publish(MQTT_PATH_PREFIX "/restart_reason/sw", get_sw_reset_reason_info(sw_reset_reason).c_str(), true);
    result &= pubsub.publish(MQTT_PATH_PREFIX "/restart_reason/run_id", String(runCounter-1).c_str(), true);

    return result;
}

bool pubsub_loop(unsigned long now) {
  return pubsub.loop(now);
}

void loop() {
  esp_task_wdt_reset();

  auto before = now;
  now = millis();
  
  if (otaUpdateMode) {
    if (now - otaUpdateStart > OTA_UPDATE_TIMEOUT_MILLIS) restart(RESET_ON_OTA_TIMEOUT);
    
    ArduinoOTA.handle();
    return;
  }

  blindsController.loop(now);

  if (wifiLoop()) {
    if (justStarted) {
      justStarted = !onJustStarted();
    }

    pubsub_loop(now);
  }

  if (now - lastOtaHandle > 2000) {
    lastOtaHandle = now;
    ArduinoOTA.handle();
  }
}

/* TOOLS */
bool parseBooleanMessage(byte* payload, unsigned int length, boolean defaultValue) {
  switch (length) {
    case 1: 
      switch (payload[0]) {
        case '1': return true;
        case '0': return false;
        default: return defaultValue;
      }
      break;
    case 2:
    case 3:
      switch (payload[1]) {
        case 'n': return true;
        case 'f': return false;
      }
      break;
    case 4: return true;
    case 5: return false;
    default: return defaultValue;
  }

  return defaultValue;
}
