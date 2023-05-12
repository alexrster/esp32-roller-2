#ifndef BLINDS_CONTROLLER_H
#define BLINDS_CONTROLLER_H

#include <Arduino.h>
#include "SwitchRelay.h"

enum BlindsState : uint8_t { 
  Unknown = 0,
  RollingUp,
  RollingDown,
  FullUp,
  FullDown
};

class BlindsController {
  public:
  typedef std::function<void()> BlindsStateChangedCallback;

    BlindsController(SwitchRelay motorUp, SwitchRelay motorDown, uint8_t edgeDetectorPin)
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
            motorUp.setOff();
            motorDown.setOff();

            if (state == RollingUp) state = FullUp;
            else if (state == RollingDown) state = FullDown;

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

      state = BlindsState::RollingUp;
      motorUp.setOn();
    }

    void pushDown() {
      if (state == BlindsState::FullDown || state == BlindsState::RollingDown) return;
      motorUp.setOff();

      state = BlindsState::RollingDown;
      motorDown.setOn();
    }

    static BlindsController create(uint8_t swUpPin, uint8_t swDownPin, uint8_t edgeDetectorPin) {
      return BlindsController(SwitchRelayPin(swUpPin), SwitchRelayPin(swDownPin), edgeDetectorPin);
    }
  
  private:
    SwitchRelay motorUp;
    SwitchRelay motorDown;
    uint8_t edgeDetectorPin;
    unsigned long lastBlindsRead = 0, lastBlindsZero = 0;
    BlindsStateChangedCallback blindsEdgeDetectedCb = NULL, blindsStateChangedCb = NULL;
    BlindsState state;
};

#endif