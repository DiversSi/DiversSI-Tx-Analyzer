
#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#include <Preferences.h>
#include <math.h>

#include "Calibration.h"

class Storage {
private:
  Preferences preferences;
  bool initialized = false;

  static constexpr const char* NAMESPACE = "diverssi";
  static constexpr const char* KEY_CALIBRATION = "calibration";
  static constexpr const char* KEY_DISPLAY = "display";

public:
  struct DisplaySettings {
    uint8_t brightness = 100;
    uint8_t theme = 0;       // 0 = Dark, 1 = Light
    uint8_t dimTimeout = 1;  // 0=30s, 1=1min, 2=2min, 3=5min
  };

private:

  struct StoredCalibration {
    uint16_t version;
    CalibrationData data;
  };

  bool validateData(const CalibrationData& data) {
    if (data.o2AirCalibrated) {
      if (!isfinite(data.o2AirVoltage_mV) ||
          data.o2AirVoltage_mV <= 0.0f) return false;
    }

    if (data.o2PureCalibrated) {
      if (!data.o2AirCalibrated ||
          !isfinite(data.o2PureVoltage_mV) ||
          data.o2PureVoltage_mV <= data.o2AirVoltage_mV)
        return false;
    }

    if (data.heZeroCalibrated &&
        !isfinite(data.heZeroVoltage_mV)) return false;

    if (data.hePureCalibrated) {
      if (!data.heZeroCalibrated ||
          !isfinite(data.hePureVoltage_mV) ||
          data.hePureVoltage_mV <= data.heZeroVoltage_mV)
        return false;
    }

    return true;
  }

public:

  bool saveDisplaySettings(const DisplaySettings& settings) {
    if (!initialized || settings.brightness < 10 || settings.brightness > 100 ||
        settings.brightness % 10 != 0 || settings.theme > 1 || settings.dimTimeout > 3)
      return false;
    return preferences.putBytes(KEY_DISPLAY, &settings, sizeof(settings)) == sizeof(settings);
  }

  bool loadDisplaySettings(DisplaySettings& settings) {
    if (!initialized || preferences.getBytesLength(KEY_DISPLAY) != sizeof(DisplaySettings))
      return false;
    DisplaySettings loaded;
    if (preferences.getBytes(KEY_DISPLAY, &loaded, sizeof(loaded)) != sizeof(loaded)) return false;
    if (loaded.brightness < 10 || loaded.brightness > 100 || loaded.brightness % 10 != 0 ||
        loaded.theme > 1 || loaded.dimTimeout > 3) return false;
    settings = loaded;
    return true;
  }

  bool begin() {
    initialized = preferences.begin(NAMESPACE, false);
    return initialized;
  }

  bool save(const Calibration& calibration) {
    if (!initialized) return false;

    StoredCalibration stored;
    stored.version = 1;
    stored.data = calibration.getData();

    if (!validateData(stored.data)) return false;

    size_t written = preferences.putBytes(
      KEY_CALIBRATION, &stored, sizeof(stored)
    );

    return written == sizeof(stored);
  }

  bool load(Calibration& calibration) {
    if (!initialized) return false;

    if (preferences.getBytesLength(KEY_CALIBRATION) !=
        sizeof(StoredCalibration)) return false;

    StoredCalibration stored;

    size_t read = preferences.getBytes(
      KEY_CALIBRATION, &stored, sizeof(stored)
    );

    if (read != sizeof(stored)) return false;
    if (stored.version != 1) return false;
    if (!validateData(stored.data)) return false;

    calibration.setData(stored.data);

    return true;
  }

  bool clear() {
    if (!initialized) return false;

    return preferences.remove(KEY_CALIBRATION);
  }

  void end() {
    if (initialized) {
      preferences.end();
      initialized = false;
    }
  }

  bool isReady() const {
    return initialized;
  }
};

#endif
