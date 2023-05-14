#ifndef PUSH_BUTTON_H
#define PUSH_BUTTON_H

#include <Arduino.h>
#include <functional>

enum class ButtonState : uint8_t { Off = 0, On };

typedef std::function<void(ButtonState)> ButtonStateCallback;

class PushButton
{
  public:
    PushButton(uint8_t pin, unsigned int threshold_ms = 100, uint8_t pinModeConfig = INPUT) : pin(pin), threshold_ms(threshold_ms)
    {
      pinMode(pin, pinModeConfig);
      setState(digitalRead(pin));
    }

    ButtonState getState()
    {
      return state;
    }

    void onButtonStateChanged(ButtonStateCallback cb)
    {
      onStateChangedCallback = cb;
    }

    void loop(unsigned long now) {
      int s = digitalRead(pin);
      if (s != (uint8_t)state) {
        if (s != (uint8_t)newState) {
          newState = (ButtonState)s;
          newState_ms = now;
        }
        else if (now - newState_ms > threshold_ms) {
          setState(s);

          if (onStateChangedCallback != NULL)
            onStateChangedCallback(state);
        }
      }
    }

  private:
    uint8_t pin;
    unsigned int threshold_ms;
    unsigned long newState_ms = 0;
    ButtonState state = ButtonState::Off, lastState = ButtonState::Off, newState = ButtonState::Off;
    ButtonStateCallback onStateChangedCallback = NULL;

    void setState(int value) {
      log_d("PushButton @%d: value=%d", pin, value);

      lastState = state;
      state = value == 0 ? ButtonState::Off : ButtonState::On;
    }
};

class ToggleButton {
  public:
    ToggleButton(uint8_t pin, unsigned int threshold_ms = 100, uint8_t pinModeConfig = INPUT, ButtonState state = ButtonState::Off) 
      : btn(pin, threshold_ms, pinModeConfig), state(state)
    { 
      btn.onButtonStateChanged([this](ButtonState s) { onPushButtonStateChanged(s); });
    }

    ButtonState getState()
    {
      return state;
    }

    void onButtonStateChanged(ButtonStateCallback cb)
    {
      onStateChangedCallback = cb;
    }

    void loop(unsigned long now) {
      btn.loop(now);
    }

  private:
    PushButton btn;
    ButtonState state = ButtonState::Off;
    ButtonStateCallback onStateChangedCallback = NULL;

    void onPushButtonStateChanged(ButtonState s) {
      if (s == ButtonState::On) {
        state = state == ButtonState::On ? ButtonState::Off : ButtonState::On;

        if (onStateChangedCallback != NULL)
          onStateChangedCallback(state);
      }
    }
};

#endif