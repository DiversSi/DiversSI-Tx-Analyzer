
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
    // Independent palette selections for Dark and Light themes.
    uint8_t darkAccent = 0, darkBackground = 0, darkText = 0;
    uint8_t lightAccent = 0, lightBackground = 0, lightText = 0;
  };

  struct LegacyDisplaySettings {
    uint8_t brightness;
    uint8_t theme;
    uint8_t dimTimeout;
  };

  static bool validDisplaySettings(const DisplaySettings& s) {
    return s.brightness >= 10 && s.brightness <= 100 && s.brightness % 10 == 0 &&
           s.theme <= 1 && s.dimTimeout <= 3 &&
           s.darkAccent <= 5 && s.lightAccent <= 5 &&
           s.darkBackground <= 4 && s.lightBackground <= 4 &&
           s.darkText <= 2 && s.lightText <= 2;
  }

  static void setDefaultPalette(DisplaySettings& s) {
    s.darkAccent = 0; s.darkBackground = 0; s.darkText = 0;
    s.lightAccent = 0; s.lightBackground = 0; s.lightText = 0;
  }

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
    if (!initialized || !validDisplaySettings(settings)) return false;
    return preferences.putBytes(KEY_DISPLAY, &settings, sizeof(settings)) == sizeof(settings);
  }

  bool loadDisplaySettings(DisplaySettings& settings) {
    if (!initialized) return false;

    const size_t storedLength = preferences.getBytesLength(KEY_DISPLAY);
    if (storedLength == sizeof(DisplaySettings)) {
      DisplaySettings loaded;
      if (preferences.getBytes(KEY_DISPLAY, &loaded, sizeof(loaded)) != sizeof(loaded) ||
          !validDisplaySettings(loaded)) return false;
      settings = loaded;
      return true;
    }

    // Migrate the original v0.4 settings (brightness/theme/dim timeout).
    if (storedLength == sizeof(LegacyDisplaySettings)) {
      LegacyDisplaySettings oldSettings;
      if (preferences.getBytes(KEY_DISPLAY, &oldSettings, sizeof(oldSettings)) != sizeof(oldSettings))
        return false;
      settings.brightness = oldSettings.brightness;
      settings.theme = oldSettings.theme;
      settings.dimTimeout = oldSettings.dimTimeout;
      setDefaultPalette(settings);
      if (!validDisplaySettings(settings)) return false;
      saveDisplaySettings(settings);
      return true;
    }

    return false;
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
