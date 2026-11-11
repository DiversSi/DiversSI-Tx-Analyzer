
#ifndef VALUE_FILTER_H
#define VALUE_FILTER_H

#include <Arduino.h>
#include <math.h>

class ValueFilter {
private:
  float filteredValue = 0.0f;
  float alpha;
  bool initialized = false;

public:
  explicit ValueFilter(float smoothing = 0.25f) {
    alpha = constrain(smoothing, 0.01f, 1.0f);
  }

  float update(float newValue) {
    if (!isfinite(newValue)) {
      return initialized ? filteredValue : NAN;
    }

    if (!initialized) {
      filteredValue = newValue;
      initialized = true;
    } else {
      filteredValue += alpha * (newValue - filteredValue);
    }

    return filteredValue;
  }

  void reset() {
    initialized = false;
    filteredValue = 0.0f;
  }

  bool isInitialized() const {
    return initialized;
  }

  float getValue() const {
    return initialized ? filteredValue : NAN;
  }
};

#endif
