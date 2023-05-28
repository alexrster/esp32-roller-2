#ifndef BLINDS_CONTROLLER_H
#define BLINDS_CONTROLLER_H

#include <Arduino.h>
#include <SwitchRelay.h>

enum class BlindsState : uint8_t { 
  Unknown = 0,
  RollingUp,
  RollingDown,
  Stopped,
  FullUp,
  FullDown,
  Obstructed
};

typedef std::function<void()> BlindsStateChangedCallback;

class BlindsController {
  public:
    BlindsController(SwitchRelayPin motorUp, SwitchRelayPin motorDown, uint8_t edgeDetectorPin, BlindsState state = BlindsState::Unknown)
      : motorUp(motorUp), motorDown(motorDown), edgeDetectorPin(edgeDetectorPin), state(state)
    { 
      pinMode(edgeDetectorPin, INPUT_PULLUP);
    }

    void onBlindsStateChanged(BlindsStateChangedCallback cb) {
      blindsStateChangedCb = cb;
    }

    bool loop(unsigned long now) {
      if (now - lastBlindsRead > 50) {
        lastBlindsRead = now;

        auto edgeDetectoprValue = digitalRead(edgeDetectorPin) == 0;
        if (edgeDetectoprValue != lastEdgeDetectorValue) {
          lastEdgeDetectorValue = edgeDetectoprValue;

          if (edgeDetectoprValue) {
            stop();

            if (state == BlindsState::RollingUp) setState(BlindsState::FullUp);
            else if (state == BlindsState::RollingDown) setState(BlindsState::FullDown);
          }
        }
        else {
          if ((state == BlindsState::RollingUp || state == BlindsState::RollingDown) && now - rollingStartTime >= BLINDS_ROLLING_TIMELIMIT_MS) {
            stop();
            setState(BlindsState::Obstructed);
          }
        }

        return true;
      }

      return false;
    }

    BlindsState getState() {
      return state;
    }

    String getStateString() {
      String stateString;
      switch (state) {
        case BlindsState::RollingUp:
          return "RollingUp";
        case BlindsState::RollingDown:
          return "RollingDown";
        case BlindsState::Stopped:
          return "Stopped";
        case BlindsState::FullUp:
          return "FullUp";
        case BlindsState::FullDown:
          return "FullDown";
        case BlindsState::Obstructed:
          return "Obstructed";
        default:
          return "Unknown";
      }
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
      setState(BlindsState::Stopped);
      motorUp.setOff();
      motorDown.setOff();
    }
  
  private:
    SwitchRelayPin motorUp;
    SwitchRelayPin motorDown;
    uint8_t edgeDetectorPin;
    BlindsState state;
    unsigned long lastBlindsRead = 0, rollingStartTime = 0;
    int lastEdgeDetectorValue = 0;
    BlindsStateChangedCallback blindsStateChangedCb = NULL;

    void setState(BlindsState s) {
      if (state == s) return;
      state = s;

      if (state == BlindsState::RollingUp || state == BlindsState::RollingDown)
        rollingStartTime = millis();

      if (blindsStateChangedCb != NULL)
        blindsStateChangedCb();
    }
};

#endif