#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include <Arduino.h>
#include "Config.h"

// A button state must remain stable for this long before it is accepted.
#define BUTTON_DEBOUNCE_US       30000UL
#define BUTTON_LONG_PRESS_US    800000UL

#define BUTTON_QUEUE_SIZE       32

enum ButtonID {
  BTN_UP,
  BTN_DOWN,
  BTN_ENTER
};

enum ButtonEventType {
  BTN_EVENT_NONE,
  BTN_EVENT_PRESSED,
  BTN_EVENT_SHORT,
  BTN_EVENT_LONG,
  BTN_EVENT_RELEASED
};

struct ButtonEvent {
  ButtonID button;
  ButtonEventType type;
};

class ButtonManager {
private:
  struct ButtonState {
    uint8_t pin;
    bool pressed;                 // Debounced/stable state
    bool candidatePressed;        // Most recently sampled raw state
    bool longSent;
    uint32_t candidateSinceUs;    // When the raw state last changed
    uint32_t pressStartUs;         // Timestamp of the accepted press edge
  };

  ButtonState buttons[3];

  ButtonEvent eventQueue[BUTTON_QUEUE_SIZE];
  uint8_t queueHead = 0;
  uint8_t queueTail = 0;

  void addEvent(ButtonID id, ButtonEventType type) {
    uint8_t next = (queueHead + 1) % BUTTON_QUEUE_SIZE;
    if (next == queueTail) {
      // Keep the existing non-blocking behavior if the event queue is full.
      return;
    }

    eventQueue[queueHead].button = id;
    eventQueue[queueHead].type = type;
    queueHead = next;
  }

public:
  void begin() {
    buttons[BTN_UP].pin = BUTTON_UP;
    buttons[BTN_DOWN].pin = BUTTON_DOWN;
    buttons[BTN_ENTER].pin = BUTTON_ENTER;

    uint32_t now = micros();

    for (uint8_t i = 0; i < 3; i++) {
      // The TTGO T4 v1.3 buttons already have external 10K pull-up resistors.
      pinMode(buttons[i].pin, INPUT);

      bool rawPressed = (digitalRead(buttons[i].pin) == BUTTON_PRESSED);
      buttons[i].pressed = rawPressed;
      buttons[i].candidatePressed = rawPressed;
      buttons[i].longSent = false;
      buttons[i].candidateSinceUs = now;
      buttons[i].pressStartUs = now;
    }

    queueHead = 0;
    queueTail = 0;
  }

  // Called repeatedly from the main loop. No GPIO interrupts are used.
  void update() {
    uint32_t now = micros();

    for (uint8_t i = 0; i < 3; i++) {
      ButtonState &b = buttons[i];
      bool rawPressed = (digitalRead(b.pin) == BUTTON_PRESSED);

      // A raw transition starts (or restarts) the debounce interval.
      if (rawPressed != b.candidatePressed) {
        b.candidatePressed = rawPressed;
        b.candidateSinceUs = now;
      }

      // Accept the new state only after it has remained stable for 30 ms.
      if (b.candidatePressed != b.pressed &&
          (uint32_t)(now - b.candidateSinceUs) >= BUTTON_DEBOUNCE_US) {

        b.pressed = b.candidatePressed;

        if (b.pressed) {
          // Confirmed press.
          b.pressStartUs = b.candidateSinceUs;
          b.longSent = false;
          addEvent((ButtonID)i, BTN_EVENT_PRESSED);
        } else {
          // Confirmed release. Classify this press exactly once.
          uint32_t duration = b.candidateSinceUs - b.pressStartUs;

          if (!b.longSent) {
            if (duration >= BUTTON_LONG_PRESS_US) {
              addEvent((ButtonID)i, BTN_EVENT_LONG);
            } else if (duration >= BUTTON_DEBOUNCE_US) {
              addEvent((ButtonID)i, BTN_EVENT_SHORT);
            }
          }

          b.longSent = false;
          addEvent((ButtonID)i, BTN_EVENT_RELEASED);
        }
      }

      // Fire LONG while the button is still stably held. If a release is
      // currently being debounced, wait for its result and classify on release.
      if (b.pressed && b.candidatePressed && !b.longSent &&
          (uint32_t)(now - b.pressStartUs) >= BUTTON_LONG_PRESS_US) {
        b.longSent = true;
        addEvent((ButtonID)i, BTN_EVENT_LONG);
      }
    }
  }

  bool available() {
    return queueHead != queueTail;
  }

  ButtonEvent read() {
    if (!available()) {
      return {BTN_UP, BTN_EVENT_NONE};
    }

    ButtonEvent event = eventQueue[queueTail];
    queueTail = (queueTail + 1) % BUTTON_QUEUE_SIZE;
    return event;
  }
};

#endif
