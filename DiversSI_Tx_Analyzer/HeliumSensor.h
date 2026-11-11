
#ifndef HELIUM_SENSOR_H
#define HELIUM_SENSOR_H

#include <Arduino.h>
#include <math.h>
#include "ADSManager.h"
#include "Calibration.h"

struct HeliumData {
  float voltage_mV;
  float heliumPercent;
  bool sensorReady;
  bool voltageValid;
  bool calibrated;
};

class HeliumSensor {
private:
  ADSManager* adc;
  Calibration* calibration;

  HeliumData data = {0.0f, NAN, false, false, false};

public:
  HeliumSensor(ADSManager& adsManager, Calibration& calibrationManager) {
    adc = &adsManager;
    calibration = &calibrationManager;
  }

  bool begin() {
    if (!adc->isReady()) {
      data.sensorReady = false;
      return false;
    }

    data.sensorReady = true;
    return true;
  }

  float readVoltage() {
    if (!data.sensorReady) return NAN;

    data.voltage_mV = adc->readChannel01mV();
    return data.voltage_mV;
  }

  HeliumData update() {
    data.voltageValid = false;

    if (!adc->isReady()) {
      data.sensorReady = false;
      return data;
    }

    data.sensorReady = true;

    float voltage = readVoltage();

    if (!isfinite(voltage)) return data;

    data.voltageValid = true;
    data.calibrated = calibration->isHeliumCalibrated();

    data.heliumPercent = calculateHelium(voltage);
    return data;
  }

  // User-specified VQ546M quadratic conversion. Input and calibration points are mV.
  float calculateHelium(float mV) const {
    const float a = 0.0214f;
    const float b = 9.6394f;

    const float U0 = calibration->getHeliumU0_mV();
    const float U100 = calibration->getHeliumU100_mV();

    if (!isfinite(mV) || !isfinite(U0) || !isfinite(U100) || U100 <= U0)
      return NAN;

    const float k = 749.94f / (U100 - U0);
    const float Mvkor = mV - U0;
    const float discriminant = b * b - 4.0f * a * k * Mvkor;

    if (!isfinite(discriminant) || discriminant < 0.0f)
      return NAN;

    const float helium = (b - sqrtf(discriminant)) / (2.0f * a);
    return isfinite(helium) ? helium : NAN;
  }

  bool isReady() {
    return data.sensorReady;
  }

  float getVoltage() {
    return data.voltage_mV;
  }
};

#endif
