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
    bool readError = false;

    bool isI2CConnected() {
        Wire.beginTransmission(ADS1115_ADDRESS);
        return Wire.endTransmission() == 0;
    }

    bool readDifferentialTimed(uint16_t mux, int16_t &result) {
        const uint32_t CONVERSION_TIMEOUT_MS = 50;

        ads.startADCReading(mux, false);

        uint32_t startTime = millis();

        while ((uint32_t)(millis() - startTime) < CONVERSION_TIMEOUT_MS) {
            if (ads.conversionComplete()) {
                result = ads.getLastConversionResults();
                return true;
            }

            yield();
        }

        return false;
    }
  public:

    bool begin() {
      Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
      Wire.setTimeout(50);

      if (!ads.begin(ADS1115_ADDRESS, &Wire)) {
        initialized = false;
        return false;
      }

      ads.setGain(ADS_GAIN_SETTING);
      initialized = true;

      return true;
    }
    /*
    bool isReady() {
      return initialized && !readError;
    }
    */
    bool isReady() {
        if (!isI2CConnected()) {
            initialized = false;
            return false;
        }

        if (!initialized) {
            if (!ads.begin(ADS1115_ADDRESS, &Wire)) {
                return false;
            }
            ads.setGain(ADS_GAIN_SETTING);
            initialized = true;
            readError = false;
        }

        return true;
    }

    int16_t readDifferential01() {
      if (!initialized) {
        readError = true;
        return 0;        
      }

      int16_t raw;

      if (!readDifferentialTimed(ADS1X15_REG_CONFIG_MUX_DIFF_0_1, raw)) {
          readError = true;
          return NAN;
      }

      return raw;
      //return ads.readADC_Differential_0_1();
    }

    int16_t readDifferential23() {
      if (!initialized ) {
        readError = true;
        return 0;
      }

      int16_t raw;

      if (!readDifferentialTimed(ADS1X15_REG_CONFIG_MUX_DIFF_2_3, raw)) {
          readError = true;
          return NAN;
      }

      return raw;
      //return ads.readADC_Differential_2_3();
    }

    float rawToMillivolts(int16_t raw) {
      return ads.computeVolts(raw) * 1000.0f;
    }

    float readChannel01mV(uint8_t samples = 1) {
      if (!initialized || samples == 0) return NAN;

      readError = false;
      int32_t sum = 0;

      for (uint8_t i = 0; i < samples; i++) {
        sum += readDifferential01();
        if (readError) return NAN;
      }

      int16_t averageRaw = sum / samples;
      return rawToMillivolts(averageRaw);
    }

    float readChannel23mV(uint8_t samples = 1) {
      if (!initialized || samples == 0) return NAN;

      readError = false;
      int32_t sum = 0;

      for (uint8_t i = 0; i < samples; i++) {
        sum += readDifferential23();
        if (readError) return NAN;
      }

      int16_t averageRaw = sum / samples;
      return rawToMillivolts(averageRaw);
    }
};

#endif

