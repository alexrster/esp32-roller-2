#ifndef SWITCH_RELAY_H
#define SWITCH_RELAY_H

#include <Arduino.h>
#include <functional>

typedef std::function<void()> SwitchRelayStateChangedCallback;

enum class SwitchState : uint8_t { Off = 0, On };

class SwitchRelay {
  public:
    SwitchRelay() { }

    void setOn() {
      setState(SwitchState::On);
    }

    void setOff() {
      setState(SwitchState::Off);
    }

    virtual SwitchState getState() { return SwitchState::Off; }
    virtual void setState(SwitchState targetState) { }

    virtual void onStateChanged(SwitchRelayStateChangedCallback cb) {
      stateChangedCallback = cb;
    }
  
  protected:
    virtual void notifyStateChanged() {
      if (stateChangedCallback != NULL)
        stateChangedCallback();
    }
  
  private:
    SwitchRelayStateChangedCallback stateChangedCallback = NULL;
};

class SwitchRelayPin : public SwitchRelay {
  public:
    SwitchRelayPin(uint8_t pin) : SwitchRelayPin(pin, 1)
    { }

    SwitchRelayPin(uint8_t pin, uint8_t onValue, uint8_t pinModeType = OUTPUT)
      : SwitchRelay(), pin(pin), onValue(onValue), offValue(onValue ? 0 : 1)
    { 
      pinMode(pin, pinModeType);
      setState(state);
    }

    virtual SwitchState getState() override {
      return state;
    }

    virtual void setState(SwitchState targetState) override {
      digitalWrite(pin, targetState == SwitchState::On ? onValue : offValue);
      state = targetState;

      notifyStateChanged();
    }
  
  private:
    const uint8_t pin, onValue, offValue;
    SwitchState state = SwitchState::Off;
};

class SwitchRelayMock : public SwitchRelay {
  public:
    SwitchRelayMock(SwitchState state = SwitchState::Off) : state(state) 
    { }

    virtual SwitchState getState() override {
      return state;
    }

    virtual void setState(SwitchState targetState) override
    { }

  private:
    SwitchState state = SwitchState::Off;
};

#endif