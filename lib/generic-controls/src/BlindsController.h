#ifndef BLINDS_CONTROLLER_H
#define BLINDS_CONTROLLER_H

#include <Arduino.h>
#include <SwitchRelay.h>

enum class BlindsState : uint8_t { 
  Unknown = 0,
  RollingUp,
  RollingDown,
  FullUp,
  FullDown
};

typedef std::function<void()> BlindsStateChangedCallback;

class BlindsController {
  public:
    BlindsController(SwitchRelayPin motorUp, SwitchRelayPin motorDown, uint8_t edgeDetectorPin)
      : motorUp(motorUp), motorDown(motorDown), edgeDetectorPin(edgeDetectorPin)
    { 
      pinMode(edgeDetectorPin, INPUT_PULLUP);
    }

    void onBlindsEdgeDetected(BlindsStateChangedCallback cb) {
      blindsEdgeDetectedCb = cb;
    }

    void onBlindsStateChanged(BlindsStateChangedCallback cb) {
      blindsEdgeDetectedCb = cb;
    }

    bool loop(unsigned long now) {
      if (now - lastBlindsRead > 50) {
        lastBlindsRead = now;

        auto blindsZero = digitalRead(edgeDetectorPin) == 0;
        if (blindsZero != lastBlindsZero) {
          lastBlindsZero = blindsZero;

          if (blindsZero) {
            stop();

            if (state == BlindsState::RollingUp) setState(BlindsState::FullUp);
            else if (state == BlindsState::RollingDown) setState(BlindsState::FullDown);

            if (blindsEdgeDetectedCb != NULL)
              blindsEdgeDetectedCb();
          }

          if (blindsStateChangedCb != NULL)
            blindsStateChangedCb();
        }

        return true;
      }

      return false;
    }

    BlindsState getState() {
      return state;
    }

    void pushUp() {
      if (state == BlindsState::FullUp || state == BlindsState::RollingUp) return;
      motorDown.setOff();

      setState(BlindsState::RollingUp);
      motorUp.setOn();
    }

    void pushDown() {
      if (state == BlindsState::FullDown || state == BlindsState::RollingDown) return;
      motorUp.setOff();

      setState(BlindsState::RollingDown);
      motorDown.setOn();
    }

    void stop() {
      motorUp.setOff();
      motorDown.setOff();
    }
  
  private:
    SwitchRelayPin motorUp;
    SwitchRelayPin motorDown;
    uint8_t edgeDetectorPin;
    unsigned long lastBlindsRead = 0, lastBlindsZero = 0;
    BlindsStateChangedCallback blindsEdgeDetectedCb = NULL, blindsStateChangedCb = NULL;
    BlindsState state;

    void setState(BlindsState s) {
      if (state == s) return;
      state = s;

      if (blindsStateChangedCb != NULL)
        blindsStateChangedCb();
    }
};

#endif