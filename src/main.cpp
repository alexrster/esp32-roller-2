#include "app.h"
#include <rom/rtc.h>
#include <esp_task_wdt.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <SwitchRelay.h>
#include <BlindsController.h>
#include "pubsub.h"
#include "reset_info.h"
#include <ArduinoOTA.h>
#include <PushButton.h>
#include <Preferences.h>

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

SwitchRelayPin swAudioPower(RELAY_BLINES_DOWN_PIN);
SwitchRelayPin relayBlindsUp(RELAY_BLINES_UP_PIN, 0);
SwitchRelayPin relayBlindsDown(RELAY_AUDIO_PIN, 0);
ToggleButton btnOnOff(BUTTON_1_PIN, 100, INPUT_PULLUP);
BlindsController blindsController(relayBlindsUp, relayBlindsDown, REEDSWITCH_1_PIN);

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

void onPubSubBlindsStateSet(uint8_t *payload, unsigned int length) { 
  if (length == 0) return;

  if (payload[0] == 'u') {
    blindsController.pushUp();
  }
  else if (payload[0] == 'd') {
    blindsController.pushDown();
  }
  else if (payload[0] == 's') {
    blindsController.stop();
  }
}

void onPubSubAudioStateSet(uint8_t *payload, unsigned int length) {
  if (length == 0) return;

  bool val = parseBooleanMessage(payload, length);
  if (val) swAudioPower.setOn();
  else swAudioPower.setOff();
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

void onButtonOnOffClick() {
  if (btnOnOff.getState() == ButtonState::Off) { // Released the button after PUSH
    pubsub.publish(MQTT_PATH_PREFIX "/button_1/state", "0");
    if (swAudioPower.getState() == SwitchState::Off)
      swAudioPower.setOn();
    else
      swAudioPower.setOff();
  }
  else {
    pubsub.publish(MQTT_PATH_PREFIX "/button_1/state", "1");
  }
}

void onAudioPowerStateChanged() {
  if (swAudioPower.getState() == SwitchState::Off) {
    digitalWrite(BUTTON_1_LED_PIN, 1);
    pubsub.publish(MQTT_PATH_PREFIX "/audio/state", "1");
  }
  else {
    digitalWrite(BUTTON_1_LED_PIN, 0);
    pubsub.publish(MQTT_PATH_PREFIX "/audio/state", "0");
  }
}

void setup() {
  reset_reason[0] = rtc_get_reset_reason(0);
  reset_reason[1] = rtc_get_reset_reason(1);

  preferences.begin("roller-02");
  runCounter = preferences.getULong("__RUN_N", 0) + 1;
  preferences.putULong("__RUN_N", runCounter);

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

  pinMode(BUTTON_1_LED_PIN, OUTPUT);

  pubsub.subscribe(MQTT_PATH_PREFIX "/restart", MQTTQOS0, onPubSubRestart);
  pubsub.subscribe(MQTT_PATH_PREFIX "/blinds/state/set", MQTTQOS0, onPubSubBlindsStateSet);
  pubsub.subscribe(MQTT_PATH_PREFIX "/audio/state/set", MQTTQOS0, onPubSubAudioStateSet);

  blindsController.onBlindsStateChanged(onBlindsStateChanged);
  btnOnOff.onChanged(onButtonOnOffClick);
  swAudioPower.onStateChanged(onAudioPowerStateChanged);

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

  result &= pubsub.publish(MQTT_PATH_PREFIX "/button_1/state", btnOnOff.getState() == ButtonState::On ? "1" : "0");
  onBlindsStateChanged();

  return result;
}

bool pubsub_loop(unsigned long now) {
  return pubsub.loop(now);
}

unsigned long blinkTime = 0;
uint8_t blinkVal = 0;
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
  btnOnOff.loop(now);

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

  if (now - blinkTime > 2000) {
    blinkTime = now;
    blinkVal = blinkVal ? 0 : 1;
    digitalWrite(BUTTON_1_LED_PIN, blinkVal);
    pubsub.publish(MQTT_PATH_PREFIX "/debug/uptime", String(now).c_str());
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
