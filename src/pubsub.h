#ifndef __PUBSUB_H
#define __PUBSUB_H

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <queue>
#include <list>
#include "app.h"

class PubSub {
  public:
    typedef std::function<void(uint8_t*, unsigned int)> message_handler_t;

    PubSub(Client& client) : pubSubClient(new PubSubClient(client))
    { 
      pubSubClient->setServer(MQTT_SERVER_NAME, MQTT_SERVER_PORT);
      pubSubClient->setCallback([this](char* t, uint8_t* p, unsigned int l) { this->mqtt_on_message(t, p, l); });
    }

    bool connect() {
      return mqtt_loop(0);
    }

    void subscribe(const char* topic, uint8_t qos, message_handler_t handler) {
      topicSubscriptions.push_back(topic_subscription_t(topic, qos, handler));
#ifdef DEBUG
      debug_publish_subscriptions();
#endif
    }

    void subscribe(const char* topic, message_handler_t handler) {
      topicSubscriptions.push_back(topic_subscription_t(topic, handler));
#ifdef DEBUG
      debug_publish_subscriptions();
#endif
    }

    bool publish(const char* topic, const char* payload, unsigned long expiresAfterMs) {
      return publish(topic, payload, false, expiresAfterMs);
    }

    bool publish(const char* topic, const char* payload, boolean retained = false, unsigned long expiresAfterMs = 0) {
      if (messageQueue.size() >= MQTT_QUEUE_MAX_SIZE) return false;

      messageQueue.push(message_t(topic, payload, retained, expiresAfterMs > 0 ? millis() + expiresAfterMs : 0));
      return true;
    }

    bool loop(unsigned long now) {
      return mqtt_loop(now) && queue_publish(now);
    }

  private:
    struct message_t {
      String topic;
      String payload;
      bool retained;
      unsigned long expires;
      uint8_t retry_counter = 0;

      message_t(const char* topic, const char* payload, boolean retained = false, unsigned long expires = 0)
        : topic(topic), payload(payload), retained(retained), expires(expires)
      { }
    };

    struct topic_subscription_t {
      String topic;
      uint8_t qos;
      message_handler_t handler;

      topic_subscription_t(const char* topic, uint8_t qos, message_handler_t handler)
        : topic(topic), qos(qos), handler(handler)
      { }

      topic_subscription_t(const char* topic, message_handler_t handler)
        : topic(topic), qos(0), handler(handler)
      { }
    };
    
    PubSubClient *pubSubClient;
    std::queue<message_t> messageQueue;
    std::queue<message_t> requeueMessages;
    std::list<topic_subscription_t> topicSubscriptions;
    unsigned long lastPubSubReconnectAttempt = 0;


#ifdef DEBUG
    unsigned long messages_sent = 0, messages_received = 0, connect_count = 0, reconnect_count = 0;
#endif

    bool reconnect(unsigned long now) {
      if (now == 0 || now - lastPubSubReconnectAttempt > MQTT_RECONNECT_MILLIS) {
        lastPubSubReconnectAttempt = now;

#ifdef DEBUG
        reconnect_count++;
#endif

        if (pubSubClient->connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD, MQTT_STATUS_TOPIC, MQTTQOS0, true, MQTT_STATUS_OFFLINE_MSG, true)) {
#ifdef DEBUG
          connect_count++;
#endif
          pubSubClient->publish(MQTT_STATUS_TOPIC, MQTT_STATUS_ONLINE_MSG, true);

#ifdef VERSION
          pubSubClient->publish(MQTT_VERSION_TOPIC, VERSION, true);
#endif

          for (auto s : topicSubscriptions) {
            pubSubClient->subscribe(s.topic.c_str(), s.qos);
          }

#ifdef DEBUG
          pubSubClient->publish(MQTT_PATH_PREFIX "/debug/pubsub/subscriptions", "");
          debug_publish_subscriptions();
#endif
        }
        
        return pubSubClient->connected();
      }

      return false;
    }

    void mqtt_on_message(char* topic, uint8_t* payload, unsigned int length) {
#ifdef DEBUG
      messages_received++;
#endif

      for (auto& s : topicSubscriptions) {
        if (s.topic.equals(topic)) {
          s.handler(payload, length);
        }
      }
    }

    bool queue_publish(unsigned long now) {
      bool result = true;
      if (messageQueue.size() == 0) return true;

#ifdef DEBUG
      pubSubClient->publish(MQTT_PATH_PREFIX "/debug/pubsub/queue_length", String(messageQueue.size()).c_str());
#endif

      while (!messageQueue.empty()) {
        auto m = messageQueue.front();
        messageQueue.pop();

        if (m.expires == 0 || (m.expires > 0 && m.expires < now)) {
          if (pubSubClient->publish(m.topic.c_str(), m.payload.c_str(), m.retained)) {
            result &= true;
#ifdef DEBUG
            messages_sent++;
#endif
          }
          else {
            result &= false;
            if (m.retry_counter++ < 3) {
              requeueMessages.push(m);
            }
          }
        }
      }

#ifdef DEBUG
      pubSubClient->publish(MQTT_PATH_PREFIX "/debug/pubsub/requeue_count", String(requeueMessages.size()).c_str());
      pubSubClient->publish(MQTT_PATH_PREFIX "/debug/pubsub/messages_sent", String(messages_sent).c_str());
      pubSubClient->publish(MQTT_PATH_PREFIX "/debug/pubsub/messages_received", String(messages_received).c_str());
      pubSubClient->publish(MQTT_PATH_PREFIX "/debug/pubsub/connect_count", String(connect_count).c_str());
      pubSubClient->publish(MQTT_PATH_PREFIX "/debug/pubsub/reconnect_count", String(reconnect_count).c_str());
#endif

      while (!requeueMessages.empty()) {
        messageQueue.push(requeueMessages.front());
        requeueMessages.pop();
      }

      return result;
    }

    bool mqtt_loop(unsigned long now) {
      if (!pubSubClient->connected() && !reconnect(now)) {
        return false;
      }

      return pubSubClient->loop();
    }

#ifdef DEBUG
    void debug_publish_subscriptions() {
      publish(MQTT_PATH_PREFIX "/debug/pubsub/subscriptions", String(topicSubscriptions.size()).c_str());
      for (auto& s : topicSubscriptions) {
        String topic(MQTT_PATH_PREFIX "/debug/pubsub/subscriptions/");
        topic.concat(s.topic);
        publish(topic.c_str(), "1");
      }
    }
#endif
};

#endif