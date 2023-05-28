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

    virtual void pushUp() { }
    virtual void pushDown() { }
    virtual void stop() { }
  
  protected:
    BlindsState state;

    BlindsController(uint8_t edgeDetectorPin, BlindsState state = BlindsState::Unknown)
      : edgeDetectorPin(edgeDetectorPin), state(state)
    { 
      pinMode(edgeDetectorPin, INPUT_PULLUP);
    }

    void setState(BlindsState s) {
      if (state == s) return;
      state = s;

      if (state == BlindsState::RollingUp || state == BlindsState::RollingDown)
        rollingStartTime = millis();

      if (blindsStateChangedCb != NULL)
        blindsStateChangedCb();
    }

  private:
    uint8_t edgeDetectorPin;
    unsigned long lastBlindsRead = 0, rollingStartTime = 0;
    int lastEdgeDetectorValue = 0;
    BlindsStateChangedCallback blindsStateChangedCb = NULL;
};

class DcMotorBlindsController : public BlindsController {
  public:
    DcMotorBlindsController(SwitchRelayPin motorUp, SwitchRelayPin motorDown, uint8_t edgeDetectorPin, BlindsState state = BlindsState::Unknown)
      : BlindsController(edgeDetectorPin, state), motorUp(motorUp), motorDown(motorDown)
    { }

    virtual void pushUp() {
      if (state == BlindsState::FullUp || state == BlindsState::RollingUp) return;
      stop();
      delay(200);

      setState(BlindsState::RollingUp);
      motorUp.setOn();
    }

    virtual void pushDown() {
      if (state == BlindsState::FullDown || state == BlindsState::RollingDown) return;
      stop();
      delay(200);

      setState(BlindsState::RollingDown);
      motorDown.setOn();
    }

    virtual void stop() {
      setState(BlindsState::Stopped);
      motorUp.setOff();
      motorDown.setOff();
    }

  private:
    SwitchRelayPin motorUp;
    SwitchRelayPin motorDown;
};

class AcMotorBlindsController : public BlindsController {
  public:
    AcMotorBlindsController(SwitchRelayPin swPower, SwitchRelayPin swDirection, uint8_t edgeDetectorPin, BlindsState state = BlindsState::Unknown)
      : BlindsController(edgeDetectorPin, state), swPower(swPower), swDirection(swDirection)
    { }

    virtual void pushUp() {
      if (state == BlindsState::FullUp || state == BlindsState::RollingUp) return;
      stop();
      delay(100);

      setState(BlindsState::RollingUp);
      swDirection.setOff(); // UP -> Off, DOWN -> On
      swPower.setOn();
    }

    virtual void pushDown() {
      if (state == BlindsState::FullDown || state == BlindsState::RollingDown) return;
      stop();
      delay(100);

      setState(BlindsState::RollingDown);
      swDirection.setOn(); // UP -> Off, DOWN -> On
      swPower.setOn();
    }

    virtual void stop() {
      setState(BlindsState::Stopped);
      swPower.setOff();
      swDirection.setOff();
    }

  private:
    SwitchRelayPin swPower;
    SwitchRelayPin swDirection;
};

#endif