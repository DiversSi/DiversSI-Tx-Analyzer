#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include <Arduino.h>
#include "Config.h"

// A state change is accepted only if at least 30 ms passed since
// the previous accepted edge of the same button.
#define BUTTON_DEBOUNCE_US       30000UL
#define BUTTON_LONG_PRESS_US    800000UL

#define BUTTON_QUEUE_SIZE       32
#define BUTTON_ISR_QUEUE_SIZE   64

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
    bool pressed;
    bool longSent;
    uint32_t pressStartUs;
  };

  struct ISRRecord {
    uint8_t button;
    bool pressed;
    uint32_t timestampUs;
  };

  ButtonState buttons[3];

  // Single-producer (ISR), single-consumer (loop) ring buffer.
  volatile ISRRecord isrQueue[BUTTON_ISR_QUEUE_SIZE];
  volatile uint8_t isrHead = 0;
  volatile uint8_t isrTail = 0;

  // Debounce is applied to accepted edges in the ISR.
  volatile uint32_t lastAcceptedEdgeUs[3] = {0, 0, 0};

  ButtonEvent eventQueue[BUTTON_QUEUE_SIZE];
  uint8_t queueHead = 0;
  uint8_t queueTail = 0;

  static ButtonManager* instance;

  void addEvent(ButtonID id, ButtonEventType type) {
    uint8_t next = (queueHead + 1) % BUTTON_QUEUE_SIZE;
    if (next == queueTail) {
      return;
    }

    eventQueue[queueHead].button = id;
    eventQueue[queueHead].type = type;
    queueHead = next;
  }

  void handleInterrupt(uint8_t index) {
    uint32_t now = micros();

    uint32_t previous = lastAcceptedEdgeUs[index];
    if (previous != 0 &&
        (uint32_t)(now - previous) < BUTTON_DEBOUNCE_US) {
      return;
    }

    bool pressed =
      (digitalRead(buttons[index].pin) == BUTTON_PRESSED);

    uint8_t next = (isrHead + 1) % BUTTON_ISR_QUEUE_SIZE;
    if (next == isrTail) {
      return; // Queue full; do not overwrite unread records.
    }

    isrQueue[isrHead].button = index;
    isrQueue[isrHead].pressed = pressed;
    isrQueue[isrHead].timestampUs = now;
    isrHead = next;

    lastAcceptedEdgeUs[index] = now;
  }

  static void isrUp() {
    if (instance) instance->handleInterrupt(BTN_UP);
  }

  static void isrDown() {
    if (instance) instance->handleInterrupt(BTN_DOWN);
  }

  static void isrEnter() {
    if (instance) instance->handleInterrupt(BTN_ENTER);
  }

public:
  void begin() {
    instance = this;

    buttons[BTN_UP].pin = BUTTON_UP;
    buttons[BTN_DOWN].pin = BUTTON_DOWN;
    buttons[BTN_ENTER].pin = BUTTON_ENTER;

    for (uint8_t i = 0; i < 3; i++) {
      pinMode(buttons[i].pin, INPUT);

      bool currentlyPressed =
        (digitalRead(buttons[i].pin) == BUTTON_PRESSED);

      buttons[i].pressed = currentlyPressed;
      buttons[i].longSent = false;
      buttons[i].pressStartUs = micros();

      lastAcceptedEdgeUs[i] = 0;
    }

    isrHead = 0;
    isrTail = 0;
    queueHead = 0;
    queueTail = 0;

    attachInterrupt(digitalPinToInterrupt(BUTTON_UP), isrUp, CHANGE);
    attachInterrupt(digitalPinToInterrupt(BUTTON_DOWN), isrDown, CHANGE);
    attachInterrupt(digitalPinToInterrupt(BUTTON_ENTER), isrEnter, CHANGE);
  }

  void update() {
    // Consume all accepted edge records in their original order.
    while (isrTail != isrHead) {
      uint8_t tail = isrTail;

      uint8_t index = isrQueue[tail].button;
      bool pressed = isrQueue[tail].pressed;
      uint32_t timestamp = isrQueue[tail].timestampUs;

      isrTail = (tail + 1) % BUTTON_ISR_QUEUE_SIZE;

      ButtonState &b = buttons[index];

      if (pressed && !b.pressed) {
        b.pressed = true;
        b.pressStartUs = timestamp;
        b.longSent = false;
        addEvent((ButtonID)index, BTN_EVENT_PRESSED);
      } else if (!pressed && b.pressed) {
        uint32_t duration = timestamp - b.pressStartUs;

        // A press is classified once: SHORT on release, or LONG
        // after the long threshold has already been reached.
        if (!b.longSent) {
          if (duration >= BUTTON_LONG_PRESS_US) {
            b.longSent = true;
            addEvent((ButtonID)index, BTN_EVENT_LONG);
          } else {
            addEvent((ButtonID)index, BTN_EVENT_SHORT);
          }
        }

        b.pressed = false;
        b.longSent = false;
        addEvent((ButtonID)index, BTN_EVENT_RELEASED);
      }
    }

    // Detect long presses even when the button has not been released.
    uint32_t now = micros();

    for (uint8_t i = 0; i < 3; i++) {
      ButtonState &b = buttons[i];

      if (b.pressed && !b.longSent &&
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

ButtonManager* ButtonManager::instance = nullptr;

#endif
