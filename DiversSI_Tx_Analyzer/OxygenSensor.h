
#ifndef OXYGEN_SENSOR_H
#define OXYGEN_SENSOR_H

#include <Arduino.h>
#include <math.h>
#include "ADSManager.h"
#include "Calibration.h"

struct OxygenData {
  float voltage_mV;
  float oxygenPercent;
  bool sensorReady;
  bool measurementValid;
  bool calibrated;
};

class OxygenSensor {
private:
  ADSManager* adc;
  Calibration* calibration;

  OxygenData data = {0.0f, 0.0f, false, false, false};

public:
  OxygenSensor(ADSManager& adsManager, Calibration& calibrationManager) {
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

    data.voltage_mV = adc->readChannel23mV();
    return data.voltage_mV;
  }

  OxygenData update() {
    data.measurementValid = false;

    if (!adc->isReady()) {
      data.sensorReady = false;
      return data;
    }

    data.sensorReady = true;

    float voltage = readVoltage();

    if (!isfinite(voltage)) return data;

    data.calibrated = calibration->isOxygenCalibrated();

    if (!data.calibrated) return data;

    data.oxygenPercent = calibration->calculateOxygen(voltage);

    data.measurementValid =
      isfinite(data.oxygenPercent) &&
      data.oxygenPercent >= 0.0f &&
      data.oxygenPercent <= 100.0f;

    return data;
  }

  bool isReady() {
    return data.sensorReady;
  }

  float getVoltage() {
    return data.voltage_mV;
  }
};

#endif
