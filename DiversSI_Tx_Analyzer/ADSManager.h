#ifndef ADS_MANAGER_H
#define ADS_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include "Config.h"

class ADSManager {
  private:
    Adafruit_ADS1115 ads;
    bool initialized = false;

  public:

    bool begin() {
      Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

      if (!ads.begin(ADS1115_ADDRESS, &Wire)) {
        initialized = false;
        return false;
      }

      ads.setGain(ADS_GAIN_SETTING);
      initialized = true;

      return true;
    }

    bool isReady() {
      return initialized;
    }

    int16_t readDifferential01() {
      if (!initialized) return 0;
      return ads.readADC_Differential_0_1();
    }

    int16_t readDifferential23() {
      if (!initialized) return 0;
      return ads.readADC_Differential_2_3();
    }

    float rawToMillivolts(int16_t raw) {
      return ads.computeVolts(raw) * 1000.0f;
    }

    float readChannel01mV(uint8_t samples = 10) {
      if (!initialized || samples == 0) return NAN;

      int32_t sum = 0;

      for (uint8_t i = 0; i < samples; i++) {
        sum += readDifferential01();
      }

      int16_t averageRaw = sum / samples;
      return rawToMillivolts(averageRaw);
    }

    float readChannel23mV(uint8_t samples = 10) {
      if (!initialized || samples == 0) return NAN;

      int32_t sum = 0;

      for (uint8_t i = 0; i < samples; i++) {
        sum += readDifferential23();
      }

      int16_t averageRaw = sum / samples;
      return rawToMillivolts(averageRaw);
    }
};

#endif

