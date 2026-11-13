
#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <Arduino.h>
#include <math.h>

#define O2_AIR_REFERENCE       20.9f
#define O2_PURE_REFERENCE     100.0f

#define HE_ZERO_REFERENCE       0.0f
#define HE_PURE_REFERENCE     100.0f

// Safe startup references used until the corresponding measured point is saved.
#define HE_DEFAULT_U0_MV        0.0f
#define HE_DEFAULT_U100_MV    749.94f

// Calibration point types are declared in a header so the Arduino IDE
// sketch preprocessor can generate function prototypes that use them.
enum OxygenCalibrationPoint {
  O2_CAL_POINT_AIR,
  O2_CAL_POINT_PURE
};

enum HeliumCalibrationPoint {
  HE_CAL_POINT_ZERO,
  HE_CAL_POINT_PURE
};

struct CalibrationData {
  float o2AirVoltage_mV;
  float o2PureVoltage_mV;

  float heZeroVoltage_mV;
  float hePureVoltage_mV;

  bool o2AirCalibrated;
  bool o2PureCalibrated;

  bool heZeroCalibrated;
  bool hePureCalibrated;
};

class Calibration {
private:
  CalibrationData data = {
    0.0f, 0.0f,
    0.0f, 0.0f,
    false, false,
    false, false
  };

public:

  // OXYGEN CALIBRATION

  bool setOxygenAir(float voltage_mV) {
    if (!isfinite(voltage_mV) || voltage_mV <= 0.0f)
      return false;

    data.o2AirVoltage_mV = voltage_mV;
    data.o2AirCalibrated = true;

    data.o2PureVoltage_mV = 0.0f;
    data.o2PureCalibrated = false;

    return true;
  }

  bool setOxygenPure(float voltage_mV) {
    if (!data.o2AirCalibrated ||
        !isfinite(voltage_mV) ||
        voltage_mV <= data.o2AirVoltage_mV)
      return false;

    data.o2PureVoltage_mV = voltage_mV;
    data.o2PureCalibrated = true;

    return true;
  }

  float calculateOxygen(float voltage_mV) const {
    if (!data.o2AirCalibrated || !isfinite(voltage_mV))
      return NAN;

    if (data.o2PureCalibrated) {
      float slope = (O2_PURE_REFERENCE - O2_AIR_REFERENCE) /
        (data.o2PureVoltage_mV - data.o2AirVoltage_mV);

      return O2_AIR_REFERENCE +
        (voltage_mV - data.o2AirVoltage_mV) * slope;
    }

    return (voltage_mV / data.o2AirVoltage_mV) *
      O2_AIR_REFERENCE;
  }

  // HELIUM CALIBRATION

  bool setHeliumZero(float voltage_mV) {
    if (!isfinite(voltage_mV))
      return false;

    data.heZeroVoltage_mV = voltage_mV;
    data.heZeroCalibrated = true;

    data.hePureVoltage_mV = 0.0f;
    data.hePureCalibrated = false;

    return true;
  }

  bool setHeliumPure(float voltage_mV) {
    if (!data.heZeroCalibrated ||
        !isfinite(voltage_mV) ||
        voltage_mV <= data.heZeroVoltage_mV)
      return false;

    data.hePureVoltage_mV = voltage_mV;
    data.hePureCalibrated = true;

    return true;
  }

  // STATUS

  bool isOxygenCalibrated() const {
    return data.o2AirCalibrated;
  }

  bool isOxygenTwoPointCalibrated() const {
    return data.o2AirCalibrated && data.o2PureCalibrated;
  }

  bool isHeliumCalibrated() const {
    return data.heZeroCalibrated && data.hePureCalibrated;
  }

  // Return measured calibration points when available, otherwise startup defaults.
  float getHeliumU0_mV() const {
    return data.heZeroCalibrated ? data.heZeroVoltage_mV : HE_DEFAULT_U0_MV;
  }

  float getHeliumU100_mV() const {
    return data.hePureCalibrated ? data.hePureVoltage_mV : HE_DEFAULT_U100_MV;
  }

  CalibrationData getData() const {
    return data;
  }

  void setData(const CalibrationData& newData) {
    data = newData;
  }
};

#endif
