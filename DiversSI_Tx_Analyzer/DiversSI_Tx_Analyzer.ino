
#include "Config.h"
#include "ADSManager.h"
#include "ButtonManager.h"
#include "DisplayManager.h"
#include "OxygenSensor.h"
#include "HeliumSensor.h"
#include "ValueFilter.h"
#include "Calibration.h"
#include "Storage.h"
#include "MenuManager.h"
#include "WifiManager.h"
#include "FirmwareUpdateManager.h"

// On-screen Wi-Fi keyboard: 36 character keys plus five action keys.
const char WIFI_SYMBOL_KEYS[] = "!@#$%^&*()-_=+[]{};:,.?/\\|<>~`\"'";
String wifiPasswordInput;
uint8_t wifiPasswordCharIndex = 0;
bool wifiKeyboardSymbols = false;
bool wifiKeyboardUppercase = false;
static const uint8_t WIFI_KEY_COUNT = 37;

String getWifiKeyboardKey(uint8_t index) {
  if (index >= 32) {
    const char* labels[] = {"SHFT", "SYM", "SPACE", "DEL", "DONE"};
    return labels[index - 32];
  }
  if (wifiKeyboardSymbols) return String(WIFI_SYMBOL_KEYS[index]);
  if (index < 26) {
    char c = wifiKeyboardUppercase ? char('A' + index) : char('a' + index);
    return String(c);
  }
  return String(char('0' + index - 26));
}

String getMaskedWifiPassword() {
  String masked;
  for (size_t i = 0; i < wifiPasswordInput.length(); ++i) masked += '*';
  return masked;
}

// Hardware modules
ADSManager adc;
ButtonManager buttons;
DisplayManager display;
Calibration calibration;
OxygenSensor oxygen(adc, calibration);
HeliumSensor helium(adc, calibration);
ValueFilter oxygenFilter(0.25f);
ValueFilter heliumFilter(0.25f);
Storage storage;
MenuManager menu;
WifiManager wifiManager;
FirmwareUpdateManager firmwareUpdate;
Storage::DisplaySettings displaySettings;
uint32_t lastDisplayActivity_ms = 0;
bool displayDimmed = false;
bool wakePressPending = false;
ButtonID wakeButton = BTN_UP;
const uint32_t DIM_TIMEOUTS_MS[] = {30000UL, 60000UL, 120000UL, 300000UL};

// IP5306 read-only status monitoring (I2C address 0x75).
static const uint8_t IP5306_ADDRESS = 0x75;
static const uint32_t BATTERY_UPDATE_INTERVAL_MS = 1000;
uint32_t lastBatteryUpdate_ms = 0;
int batteryPercent = -1;
bool batteryCharging = false;
bool batteryFull = false;
bool batteryAvailable = false;

bool readIP5306Register(uint8_t reg, uint8_t &value) {
  Wire.beginTransmission(IP5306_ADDRESS);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)IP5306_ADDRESS, (uint8_t)1) != 1) return false;
  value = Wire.read();
  return true;
}

void updateBatteryStatus(bool force = false) {
  const uint32_t now = millis();
  if (!force && (uint32_t)(now - lastBatteryUpdate_ms) < BATTERY_UPDATE_INTERVAL_MS) return;
  lastBatteryUpdate_ms = now;

  uint8_t gauge = 0, charge = 0, full = 0;
  bool gaugeOk = readIP5306Register(0x78, gauge);
  bool chargeOk = readIP5306Register(0x70, charge);
  bool fullOk = readIP5306Register(0x71, full);
  batteryAvailable = gaugeOk;
  batteryPercent = -1;

  if (gaugeOk) {
    switch ((gauge >> 4) & 0x0F) {
      case 0x0: batteryPercent = 0; break;
      case 0x1: batteryPercent = 25; break;
      case 0x3: batteryPercent = 50; break;
      case 0x7: batteryPercent = 75; break;
      case 0xF: batteryPercent = 100; break;
      default: batteryPercent = -1; break;
    }
  }
  batteryCharging = chargeOk && (charge & 0x08);
  batteryFull = fullOk && (full & 0x08);

  if (menu.getScreen() == SCREEN_MAIN) {
    display.updateBatteryIndicator(batteryPercent, batteryCharging, batteryFull);
  }
}


// Non-blocking two-point O2 calibration state (AIR and PURE).
enum OxygenCalibrationState {
  O2_CAL_IDLE,
  O2_CAL_CAPTURING,
  O2_CAL_FAILED,
  O2_CAL_SUCCEEDED,
  O2_CAL_BLOCKED
};

enum HeliumCalibrationState {
  HE_CAL_IDLE,
  HE_CAL_CAPTURING,
  HE_CAL_FAILED,
  HE_CAL_SUCCEEDED,
  HE_CAL_BLOCKED
};

static const uint8_t O2_CAL_SAMPLE_COUNT = 10;
static const uint8_t O2_CAL_ADC_READS_PER_SAMPLE = 10;
static const uint32_t O2_CAL_SAMPLE_INTERVAL_MS = 500;
static const float O2_CAL_STABILITY_LIMIT_PERCENT = 1.0f;
static const uint32_t O2_AIR_TO_PURE_WINDOW_MS = 5UL * 60UL * 1000UL;
static const uint32_t HE_ZERO_TO_PURE_WINDOW_MS = 5UL * 60UL * 1000UL;

static const uint8_t HE_CAL_SAMPLE_COUNT = O2_CAL_SAMPLE_COUNT;
static const uint8_t HE_CAL_ADC_READS_PER_SAMPLE = O2_CAL_ADC_READS_PER_SAMPLE;
static const uint32_t HE_CAL_SAMPLE_INTERVAL_MS = O2_CAL_SAMPLE_INTERVAL_MS;
static const float HE_CAL_STABILITY_LIMIT_PERCENT = O2_CAL_STABILITY_LIMIT_PERCENT;

OxygenCalibrationState o2CalibrationState = O2_CAL_IDLE;
OxygenCalibrationPoint o2CalibrationPoint = O2_CAL_POINT_AIR;
float o2CalibrationSamples[O2_CAL_SAMPLE_COUNT];
uint8_t o2CalibrationSampleIndex = 0;
uint32_t o2NextSampleTime = 0;
float o2CalibrationAverage_mV = NAN;
float o2CalibrationRange_mV = NAN;

// AIR-to-PURE timing is intentionally valid only during this boot session.
bool o2AirCalibratedThisBoot = false;
uint32_t o2AirCalibrationTime_ms = 0;

HeliumCalibrationState heCalibrationState = HE_CAL_IDLE;
HeliumCalibrationPoint heCalibrationPoint = HE_CAL_POINT_ZERO;
float heCalibrationSamples[HE_CAL_SAMPLE_COUNT];
uint8_t heCalibrationSampleIndex = 0;
uint32_t heNextSampleTime = 0;
float heCalibrationAverage_mV = NAN;
float heCalibrationRange_mV = NAN;

// ZERO-to-PURE timing is valid only during this boot session.
bool heZeroCalibratedThisBoot = false;
uint32_t heZeroCalibrationTime_ms = 0;

// Normal sensor polling scheduler.
static const uint32_t SENSOR_UPDATE_INTERVAL_MS = 100;
uint32_t lastSensorUpdate_ms = 0;
float latestFilteredO2 = NAN;
float latestFilteredHePercent = NAN;


const char* getOxygenCalibrationName() {
  return (o2CalibrationPoint == O2_CAL_POINT_AIR) ? "AIR 20.9 %" : "PURE 100 %";
}

void startOxygenCalibration(OxygenCalibrationPoint point) {
  o2CalibrationPoint = point;
  o2CalibrationSampleIndex = 0;
  o2CalibrationAverage_mV = NAN;
  o2CalibrationRange_mV = NAN;
  o2NextSampleTime = millis() + O2_CAL_SAMPLE_INTERVAL_MS;
  o2CalibrationState = O2_CAL_CAPTURING;

  Serial.println();
  Serial.printf("O2 %s calibration started.\n", getOxygenCalibrationName());
  Serial.println("Sampling every 500 ms; 10 ADC readings per sample.");

  display.drawOxygenCalibrationProgress(
    getOxygenCalibrationName(),
    0, O2_CAL_SAMPLE_COUNT, NAN
  );
}

void finishOxygenCalibration() {
  float sum = 0.0f;
  float minimum = o2CalibrationSamples[0];
  float maximum = o2CalibrationSamples[0];

  for (uint8_t i = 0; i < O2_CAL_SAMPLE_COUNT; i++) {
    float value = o2CalibrationSamples[i];
    sum += value;

    if (value < minimum) minimum = value;
    if (value > maximum) maximum = value;
  }

  o2CalibrationAverage_mV = sum / O2_CAL_SAMPLE_COUNT;
  o2CalibrationRange_mV = maximum - minimum;

  const float allowedRange_mV =
    fabsf(o2CalibrationAverage_mV) *
    O2_CAL_STABILITY_LIMIT_PERCENT / 100.0f;

  Serial.printf(
    "O2 %s calibration: average=%.3f mV, range=%.3f mV, allowed=%.3f mV\n",
    getOxygenCalibrationName(),
    o2CalibrationAverage_mV,
    o2CalibrationRange_mV,
    allowedRange_mV
  );

  if (!isfinite(o2CalibrationAverage_mV) ||
      o2CalibrationAverage_mV <= 0.0f ||
      o2CalibrationRange_mV > allowedRange_mV) {
    o2CalibrationState = O2_CAL_FAILED;
    display.drawOxygenCalibrationResult(
      getOxygenCalibrationName(),
      false,
      o2CalibrationAverage_mV,
      o2CalibrationRange_mV,
      "Unstable measurements"
    );
    Serial.printf("O2 %s calibration FAILED: unstable measurements.\n",
                  getOxygenCalibrationName());
    return;
  }

  // Keep the current calibration if validation or NVS saving fails.
  CalibrationData previousCalibration = calibration.getData();

  bool calibrationAccepted = false;
  if (o2CalibrationPoint == O2_CAL_POINT_AIR) {
    calibrationAccepted = calibration.setOxygenAir(o2CalibrationAverage_mV);
  } else {
    calibrationAccepted = calibration.setOxygenPure(o2CalibrationAverage_mV);
  }

  if (!calibrationAccepted) {
    o2CalibrationState = O2_CAL_FAILED;

    const char* errorText =
      (o2CalibrationPoint == O2_CAL_POINT_PURE)
        ? "PURE voltage must exceed AIR"
        : "Invalid voltage";

    display.drawOxygenCalibrationResult(
      getOxygenCalibrationName(),
      false,
      o2CalibrationAverage_mV,
      o2CalibrationRange_mV,
      errorText
    );
    Serial.printf("O2 %s calibration FAILED: %s.\n",
                  getOxygenCalibrationName(), errorText);
    return;
  }

  if (!storage.save(calibration)) {
    calibration.setData(previousCalibration);
    o2CalibrationState = O2_CAL_FAILED;
    display.drawOxygenCalibrationResult(
      getOxygenCalibrationName(),
      false,
      o2CalibrationAverage_mV,
      o2CalibrationRange_mV,
      "Storage error"
    );
    Serial.printf("O2 %s calibration FAILED: NVS save error.\n",
                  getOxygenCalibrationName());
    return;
  }

  if (o2CalibrationPoint == O2_CAL_POINT_AIR) {
    o2AirCalibrationTime_ms = millis();
    o2AirCalibratedThisBoot = true;
    Serial.println("O2 AIR calibration time window started (5 minutes).");
  }

  o2CalibrationState = O2_CAL_SUCCEEDED;
  display.drawOxygenCalibrationResult(
    getOxygenCalibrationName(),
    true,
    o2CalibrationAverage_mV,
    o2CalibrationRange_mV,
    "Stable measurement"
  );
  Serial.printf("O2 %s calibration SAVED.\n", getOxygenCalibrationName());
}

void updateOxygenCalibration() {
  if (o2CalibrationState != O2_CAL_CAPTURING) return;

  uint32_t now = millis();
  if ((int32_t)(now - o2NextSampleTime) < 0) return;

  // The O2 sensor is connected to ADS1115 differential input A2-A3.
  float sample_mV = adc.readChannel23mV(O2_CAL_ADC_READS_PER_SAMPLE);

  if (!isfinite(sample_mV)) {
    o2CalibrationState = O2_CAL_FAILED;
    display.drawOxygenCalibrationResult(
      getOxygenCalibrationName(), false, NAN, NAN, "ADC read error"
    );
    Serial.printf("O2 %s calibration FAILED: ADC read error.\n",
                  getOxygenCalibrationName());
    return;
  }

  o2CalibrationSamples[o2CalibrationSampleIndex] = sample_mV;
  o2CalibrationSampleIndex++;

  Serial.printf(
    "O2 %s sample %u/%u: %.3f mV\n",
    getOxygenCalibrationName(),
    o2CalibrationSampleIndex,
    O2_CAL_SAMPLE_COUNT,
    sample_mV
  );

  display.drawOxygenCalibrationProgress(
    getOxygenCalibrationName(),
    o2CalibrationSampleIndex,
    O2_CAL_SAMPLE_COUNT,
    sample_mV
  );

  if (o2CalibrationSampleIndex >= O2_CAL_SAMPLE_COUNT) {
    finishOxygenCalibration();
  } else {
    // Schedule the next sample 500 ms after this sample has completed.
    o2NextSampleTime = millis() + O2_CAL_SAMPLE_INTERVAL_MS;
  }
}


const char* getHeliumCalibrationName() {
  return (heCalibrationPoint == HE_CAL_POINT_ZERO) ? "ZERO 0 % (AIR)" : "PURE 100 %";
}

void startHeliumCalibration(HeliumCalibrationPoint point) {
  heCalibrationPoint = point;
  heCalibrationSampleIndex = 0;
  heCalibrationAverage_mV = NAN;
  heCalibrationRange_mV = NAN;
  heNextSampleTime = millis() + HE_CAL_SAMPLE_INTERVAL_MS;
  heCalibrationState = HE_CAL_CAPTURING;

  Serial.println();
  Serial.printf("He %s calibration started.\n", getHeliumCalibrationName());
  Serial.println("Sampling every 500 ms; 10 ADC readings per sample.");

  display.drawHeliumCalibrationProgress(getHeliumCalibrationName(), 0, HE_CAL_SAMPLE_COUNT, NAN);
}

void finishHeliumCalibration() {
  float sum = 0.0f;
  float minimum = heCalibrationSamples[0];
  float maximum = heCalibrationSamples[0];

  for (uint8_t i = 0; i < HE_CAL_SAMPLE_COUNT; i++) {
    float value = heCalibrationSamples[i];
    sum += value;
    if (value < minimum) minimum = value;
    if (value > maximum) maximum = value;
  }

  heCalibrationAverage_mV = sum / HE_CAL_SAMPLE_COUNT;
  heCalibrationRange_mV = maximum - minimum;

  // Use the magnitude of the mean so a negative ZERO voltage is valid.
  const float allowedRange_mV = fabsf(heCalibrationAverage_mV) *
    HE_CAL_STABILITY_LIMIT_PERCENT / 100.0f;

  Serial.printf("He %s calibration: average=%.3f mV, range=%.3f mV, allowed=%.3f mV\n",
                getHeliumCalibrationName(), heCalibrationAverage_mV,
                heCalibrationRange_mV, allowedRange_mV);

  if (!isfinite(heCalibrationAverage_mV) ||
      heCalibrationRange_mV > allowedRange_mV) {
    heCalibrationState = HE_CAL_FAILED;
    display.drawHeliumCalibrationResult(getHeliumCalibrationName(), false,
      heCalibrationAverage_mV, heCalibrationRange_mV, "Unstable measurements");
    Serial.printf("He %s calibration FAILED: unstable measurements.\n", getHeliumCalibrationName());
    return;
  }

  CalibrationData previousCalibration = calibration.getData();
  bool calibrationAccepted = false;
  if (heCalibrationPoint == HE_CAL_POINT_ZERO) {
    calibrationAccepted = calibration.setHeliumZero(heCalibrationAverage_mV);
  } else {
    calibrationAccepted = calibration.setHeliumPure(heCalibrationAverage_mV);
  }

  if (!calibrationAccepted) {
    heCalibrationState = HE_CAL_FAILED;
    const char* errorText = (heCalibrationPoint == HE_CAL_POINT_PURE)
      ? "PURE voltage must exceed ZERO" : "Invalid voltage";
    display.drawHeliumCalibrationResult(getHeliumCalibrationName(), false,
      heCalibrationAverage_mV, heCalibrationRange_mV, errorText);
    Serial.printf("He %s calibration FAILED: %s.\n", getHeliumCalibrationName(), errorText);
    return;
  }

  if (!storage.save(calibration)) {
    calibration.setData(previousCalibration);
    heCalibrationState = HE_CAL_FAILED;
    display.drawHeliumCalibrationResult(getHeliumCalibrationName(), false,
      heCalibrationAverage_mV, heCalibrationRange_mV, "Storage error");
    Serial.printf("He %s calibration FAILED: NVS save error.\n", getHeliumCalibrationName());
    return;
  }

  if (heCalibrationPoint == HE_CAL_POINT_ZERO) {
    heZeroCalibrationTime_ms = millis();
    heZeroCalibratedThisBoot = true;
    Serial.println("He ZERO calibration time window started (5 minutes).");
  }

  heCalibrationState = HE_CAL_SUCCEEDED;
  display.drawHeliumCalibrationResult(getHeliumCalibrationName(), true,
    heCalibrationAverage_mV, heCalibrationRange_mV, "Stable measurement");
  Serial.printf("He %s calibration SAVED.\n", getHeliumCalibrationName());
}

void updateHeliumCalibration() {
  if (heCalibrationState != HE_CAL_CAPTURING) return;

  uint32_t now = millis();
  if ((int32_t)(now - heNextSampleTime) < 0) return;

  // The VQ546M is connected to ADS1115 differential input A0-A1.
  float sample_mV = adc.readChannel01mV(HE_CAL_ADC_READS_PER_SAMPLE);
  if (!isfinite(sample_mV)) {
    heCalibrationState = HE_CAL_FAILED;
    display.drawHeliumCalibrationResult(getHeliumCalibrationName(), false, NAN, NAN, "ADC read error");
    Serial.printf("He %s calibration FAILED: ADC read error.\n", getHeliumCalibrationName());
    return;
  }

  heCalibrationSamples[heCalibrationSampleIndex] = sample_mV;
  heCalibrationSampleIndex++;
  Serial.printf("He %s sample %u/%u: %.3f mV\n", getHeliumCalibrationName(),
                heCalibrationSampleIndex, HE_CAL_SAMPLE_COUNT, sample_mV);
  display.drawHeliumCalibrationProgress(getHeliumCalibrationName(),
    heCalibrationSampleIndex, HE_CAL_SAMPLE_COUNT, sample_mV);

  if (heCalibrationSampleIndex >= HE_CAL_SAMPLE_COUNT) {
    finishHeliumCalibration();
  } else {
    heNextSampleTime = millis() + HE_CAL_SAMPLE_INTERVAL_MS;
  }
}


void setup() {

  Serial.begin(SERIAL_BAUDRATE);
  delay(1000);

  Serial.println();
  Serial.println(PROJECT_NAME);
  Serial.println("Version: " PROJECT_VERSION);

  // Initialize ADS1115
  bool adcReady = adc.begin();

  if (adcReady) {
    Serial.println("ADS1115: OK");
  } else {
    Serial.println("ADS1115: ERROR");
  }
  
  // Initialize storage
  if (storage.begin()) {
    if (storage.load(calibration)) {
      Serial.println("Calibration: LOADED");
    } else {
      Serial.println("Calibration: NOT FOUND");
    }
  } else {
    Serial.println("Storage: ERROR");
  }
  // Initialize sensors
  oxygen.begin();
  helium.begin();
  
  // Initialize buttons
  buttons.begin();
  Serial.println("Buttons: OK");

  // Initialize display and restore user display preferences.
  if (!storage.loadDisplaySettings(displaySettings)) {
    displaySettings.brightness = 100;
    displaySettings.theme = 0;
    displaySettings.dimTimeout = 1;
    Storage::setDefaultPalette(displaySettings);
  }
  display.begin();
  display.setTheme(displaySettings.theme == 1,
                   displaySettings.theme ? displaySettings.lightAccent : displaySettings.darkAccent,
                   displaySettings.theme ? displaySettings.lightBackground : displaySettings.darkBackground,
                   displaySettings.theme ? displaySettings.lightText : displaySettings.darkText);
  display.setBrightness(displaySettings.brightness);
  lastDisplayActivity_ms = millis();

  wifiManager.begin();
  menu.begin();
  display.drawMainScreen(adcReady);
  updateBatteryStatus(true);
  menu.clearScreenChanged();
  Serial.println("Display: OK");

  Serial.println("System initialization complete.");
}


void loop() {

  // Read IP5306 status periodically; all accesses are read-only.
  updateBatteryStatus();

  // Wi-Fi scanning and connection run asynchronously so sensor updates continue.
  WifiManager::ScanState previousWifiState = wifiManager.getState();
  wifiManager.update();
  if (previousWifiState != wifiManager.getState()) {
    Serial.printf("[WiFi-DIAG][APP] scan state transition %d -> %d at t=%lu ms; screen=%d\n",
                  (int)previousWifiState, (int)wifiManager.getState(),
                  (unsigned long)millis(), (int)menu.getScreen());
  }
  if (previousWifiState == WifiManager::WIFI_SCANNING &&
      wifiManager.getState() != WifiManager::WIFI_SCANNING) {
    bool autoStarted = false;
    if (wifiManager.getState() == WifiManager::WIFI_READY) {
      Serial.println("[WiFi-DIAG][APP] scan ready; checking saved network");
      autoStarted = wifiManager.connectSavedIfFound();
    } else {
      Serial.printf("[WiFi-DIAG][APP] scan ended in state=%d; no auto-connect\n",
                    (int)wifiManager.getState());
    }
    Serial.printf("[WiFi-DIAG][APP] autoStarted=%s; network count=%u\n",
                  autoStarted ? "true" : "false", wifiManager.getCount());
    // If no saved network is visible (or scanning failed), let the user
    // choose from the discovered networks instead of leaving Wi-Fi stuck ON.
    if (!autoStarted && menu.getScreen() == SCREEN_SYSTEM_SETTINGS && wifiManager.isEnabled()) {
      menu.openWifiNetworks();
    }
  }
  static bool lastWifiConnectedForMenu = false;
  static bool lastWifiEnabledForMenu = false;
  static bool lastWifiBusyForMenu = false;
  if (menu.getScreen() == SCREEN_SYSTEM_SETTINGS &&
      (lastWifiConnectedForMenu != wifiManager.isConnected() ||
       lastWifiEnabledForMenu != wifiManager.isEnabled() ||
       lastWifiBusyForMenu != wifiManager.isBusy())) {
    display.updateSystemSettingsWifiStatus(menu.getSelectedItem(), wifiManager.isEnabled(),
                                           wifiManager.isConnected(), wifiManager.isBusy());
  }
  lastWifiConnectedForMenu = wifiManager.isConnected();
  lastWifiEnabledForMenu = wifiManager.isEnabled();
  lastWifiBusyForMenu = wifiManager.isBusy();

  // A failed automatic connection returns the user to the available network list.
  static WifiManager::ConnectState previousWifiConnectState = WifiManager::CONNECT_IDLE;
  WifiManager::ConnectState currentWifiConnectState = wifiManager.getConnectState();
  if (previousWifiConnectState == WifiManager::CONNECTING &&
      currentWifiConnectState == WifiManager::CONNECT_FAILED &&
      menu.getScreen() == SCREEN_SYSTEM_SETTINGS && wifiManager.isEnabled()) {
    menu.openWifiNetworks();
  }
  if (previousWifiConnectState == WifiManager::CONNECTING &&
      currentWifiConnectState == WifiManager::CONNECTED &&
      menu.getScreen() == SCREEN_WIFI_PASSWORD) {
    Serial.println("[WiFi][APP] connection successful; returning to System Settings");
    menu.returnToSystemSettings();
  }
  previousWifiConnectState = currentWifiConnectState;

  if (menu.getScreen() == SCREEN_WIFI_NETWORKS) {
    if (previousWifiState == WifiManager::WIFI_SCANNING &&
        wifiManager.getState() != WifiManager::WIFI_SCANNING) {
      menu.setWifiNetworkCount(wifiManager.getCount());
      // Draw network rows only after passing valid arrays. Passing nullptr here
      // crashes when at least one network is found.
      String names[WifiManager::MAX_NETWORKS];
      int32_t signals[WifiManager::MAX_NETWORKS];
      for (uint8_t i = 0; i < wifiManager.getCount(); ++i) {
        names[i] = wifiManager.getSSID(i);
        signals[i] = wifiManager.getRSSI(i);
      }
      display.drawWifiNetworks(menu.getSelectedItem(), wifiManager.getCount(), false,
        wifiManager.getState() == WifiManager::WIFI_FAILED, names, signals);
    }
  }

  // Process buttons on every loop pass.
  buttons.update();

  while (buttons.available()) {

    ButtonEvent event = buttons.read();

    // Dedicated three-button Wi-Fi password entry.
    if (menu.getScreen() == SCREEN_WIFI_PASSWORD) {
      if (event.button == BTN_ENTER && event.type == BTN_EVENT_LONG) {
        wifiManager.cancelConnection();
        wifiPasswordInput = "";
        menu.returnToSystemSettings();
        continue;
      }
      if (event.type == BTN_EVENT_SHORT) {
        if (wifiManager.getConnectState() == WifiManager::CONNECTING) continue;
        if (wifiManager.getConnectState() == WifiManager::CONNECTED) {
          menu.backToWifiNetworks();
          continue;
        }
        if (wifiManager.getConnectState() == WifiManager::CONNECT_FAILED) {
          if (event.button == BTN_ENTER) {
            wifiManager.connectSelected(wifiPasswordInput);
            continue;
          }
          wifiManager.clearConnectResult();
        }
        if (event.button == BTN_UP) {
          wifiPasswordCharIndex = (wifiPasswordCharIndex + 1) % WIFI_KEY_COUNT;
        } else if (event.button == BTN_DOWN) {
          wifiPasswordCharIndex = (wifiPasswordCharIndex == 0) ? WIFI_KEY_COUNT - 1 : wifiPasswordCharIndex - 1;
        } else if (event.button == BTN_ENTER) {
          if (wifiPasswordCharIndex < 32) {
            if (wifiPasswordInput.length() < 63) {
              wifiPasswordInput += getWifiKeyboardKey(wifiPasswordCharIndex);
            }
          } else if (wifiPasswordCharIndex == 32) {
            wifiKeyboardUppercase = !wifiKeyboardUppercase;
          } else if (wifiPasswordCharIndex == 33) {
            wifiKeyboardSymbols = !wifiKeyboardSymbols;
          } else if (wifiPasswordCharIndex == 34) {
            if (wifiPasswordInput.length() < 63) wifiPasswordInput += ' ';
          } else if (wifiPasswordCharIndex == 35) {
            if (wifiPasswordInput.length()) wifiPasswordInput.remove(wifiPasswordInput.length() - 1);
          } else if (wifiPasswordCharIndex == 36) {
            if (wifiPasswordInput.length() >= 8 && wifiPasswordInput.length() <= 63) {
              wifiManager.connectSelected(wifiPasswordInput);
            }
          }
        }
        String masked = getMaskedWifiPassword();
        display.drawWifiPassword(wifiManager.getSelectedSSID(), masked,
          wifiPasswordCharIndex, wifiKeyboardSymbols, wifiKeyboardUppercase,
          wifiManager.getConnectState() == WifiManager::CONNECTING,
          wifiManager.getConnectState() == WifiManager::CONNECTED,
          wifiManager.getConnectState() == WifiManager::CONNECT_FAILED);
      }
      continue;
    }

    // A press on the dimmed main measurement screen wakes the display only.
    if (menu.getScreen() == SCREEN_MAIN && displayDimmed &&
        event.type == BTN_EVENT_PRESSED) {
      displayDimmed = false;
      display.setDimmed(false);
      lastDisplayActivity_ms = millis();
      wakePressPending = true;
      wakeButton = event.button;
      continue;
    }
    if (wakePressPending && event.button == wakeButton) {
      if (event.type == BTN_EVENT_SHORT || event.type == BTN_EVENT_LONG) {
        wakePressPending = false;
      }
      continue;
    }
    if (event.type == BTN_EVENT_PRESSED && menu.getScreen() == SCREEN_MAIN) {
      lastDisplayActivity_ms = millis();
    }

    const char* buttonName = "";

    switch (event.button) {
      case BTN_UP:    buttonName = "UP";    break;
      case BTN_DOWN:  buttonName = "DOWN";  break;
      case BTN_ENTER: buttonName = "ENTER"; break;
    }

    // While either calibration screen is active, ENTER is handled here
    // instead of by MenuManager so its long-press event cannot leave the flow.
    if (o2CalibrationState != O2_CAL_IDLE || heCalibrationState != HE_CAL_IDLE) {
      if (event.button == BTN_ENTER && event.type == BTN_EVENT_LONG) {
        if (o2CalibrationState != O2_CAL_IDLE) {
          o2CalibrationState = O2_CAL_IDLE;
          Serial.println("O2 calibration cancelled.");
        }
        if (heCalibrationState != HE_CAL_IDLE) {
          heCalibrationState = HE_CAL_IDLE;
          Serial.println("He calibration cancelled.");
        }
        display.drawCalibrationMenu(menu.getSelectedItem());
      } else if (event.button == BTN_ENTER && event.type == BTN_EVENT_SHORT) {
        if (o2CalibrationState == O2_CAL_FAILED) {
          startOxygenCalibration(o2CalibrationPoint);
        } else if (o2CalibrationState == O2_CAL_SUCCEEDED ||
                   o2CalibrationState == O2_CAL_BLOCKED) {
          o2CalibrationState = O2_CAL_IDLE;
          display.drawCalibrationMenu(menu.getSelectedItem());
        } else if (heCalibrationState == HE_CAL_FAILED) {
          startHeliumCalibration(heCalibrationPoint);
        } else if (heCalibrationState == HE_CAL_SUCCEEDED ||
                   heCalibrationState == HE_CAL_BLOCKED) {
          heCalibrationState = HE_CAL_IDLE;
          display.drawCalibrationMenu(menu.getSelectedItem());
        }
      }
      continue;
    }

    AppScreen screenBeforeEvent = menu.getScreen();
    if (screenBeforeEvent == SCREEN_WIFI_NETWORKS &&
        event.type == BTN_EVENT_SHORT && event.button == BTN_ENTER) {
      if (wifiManager.getState() == WifiManager::WIFI_FAILED || wifiManager.getCount() == 0) {
        Serial.printf("[WiFi-DIAG][UI] ENTER retry scan; state=%d count=%u at t=%lu ms\n",
                      (int)wifiManager.getState(), wifiManager.getCount(), (unsigned long)millis());
        wifiManager.beginScan();
        display.drawWifiNetworks(0, 0, true, false, nullptr, nullptr);
      } else {
        uint8_t chosen = menu.getSelectedItem();
        wifiPasswordInput = "";
        wifiPasswordCharIndex = 0;
        wifiKeyboardSymbols = false;
        wifiKeyboardUppercase = false;
        if (wifiManager.isEncrypted(chosen)) {
          wifiManager.selectNetwork(chosen);
          menu.openWifiPassword();
        } else {
          wifiManager.selectNetwork(chosen);
          wifiManager.connectTo(chosen, "");
          menu.openWifiPassword();
        }
      }
      continue;
    }

    if (screenBeforeEvent == SCREEN_SYSTEM_SETTINGS &&
        event.type == BTN_EVENT_SHORT && event.button == BTN_ENTER &&
        menu.getSelectedItem() == 0) {
      if (wifiManager.isEnabled()) {
        wifiManager.setEnabled(false);
      } else {
        wifiManager.setEnabled(true);
      }
      display.updateSystemSettingsWifiStatus(menu.getSelectedItem(), wifiManager.isEnabled(), wifiManager.isConnected(), wifiManager.isBusy());
      continue;
    }

    if (screenBeforeEvent == SCREEN_FIRMWARE_UPDATE &&
        event.type == BTN_EVENT_SHORT && event.button == BTN_ENTER) {
      if (firmwareUpdate.isReadyToInstall()) {
        display.drawFirmwareUpdateInfo(
          firmwareUpdate.hasChecked(), firmwareUpdate.isSuccessful(),
          firmwareUpdate.getLatestVersion(), "Downloading and verifying firmware");
        firmwareUpdate.installUpdate(wifiManager.isConnected());
      } else {
        firmwareUpdate.checkLatest(wifiManager.isConnected());
      }
      display.drawFirmwareUpdateInfo(
        firmwareUpdate.hasChecked(), firmwareUpdate.isSuccessful(),
        firmwareUpdate.getLatestVersion(), firmwareUpdate.getStatusMessage());
      continue;
    }

    if (screenBeforeEvent == SCREEN_DISPLAY_SETTINGS &&
        event.type == BTN_EVENT_SHORT && event.button == BTN_ENTER) {
      uint8_t selected = menu.getSelectedItem();
      if (selected == 0) {
        displaySettings.brightness = (displaySettings.brightness >= 100) ? 10 : displaySettings.brightness + 10;
      } else if (selected == 1) {
        displaySettings.theme = displaySettings.theme ? 0 : 1;
      } else if (selected == 2) {
        displaySettings.dimTimeout = (displaySettings.dimTimeout + 1) % 4;
      } else if (displaySettings.theme == 0 && selected == 3) {
        displaySettings.darkAccent = (displaySettings.darkAccent + 1) % 6;
      } else if (displaySettings.theme == 0 && selected == 4) {
        displaySettings.darkBackground = (displaySettings.darkBackground + 1) % 5;
      } else if (displaySettings.theme == 0 && selected == 5) {
        displaySettings.darkText = (displaySettings.darkText + 1) % 3;
      } else if (displaySettings.theme == 1 && selected == 3) {
        displaySettings.lightAccent = (displaySettings.lightAccent + 1) % 6;
      } else if (displaySettings.theme == 1 && selected == 4) {
        displaySettings.lightBackground = (displaySettings.lightBackground + 1) % 5;
      } else if (displaySettings.theme == 1 && selected == 5) {
        displaySettings.lightText = (displaySettings.lightText + 1) % 3;
      }
      uint8_t activeAccent = displaySettings.theme ? displaySettings.lightAccent : displaySettings.darkAccent;
      uint8_t activeBackground = displaySettings.theme ? displaySettings.lightBackground : displaySettings.darkBackground;
      uint8_t activeText = displaySettings.theme ? displaySettings.lightText : displaySettings.darkText;
      display.setTheme(displaySettings.theme == 1, activeAccent, activeBackground, activeText);
      display.setBrightness(displaySettings.brightness);
      storage.saveDisplaySettings(displaySettings);
      if (selected == 0 || selected == 2) {
        display.updateDisplaySettingsValue(selected, displaySettings.brightness, displaySettings.theme,
          displaySettings.dimTimeout, activeAccent, activeBackground, activeText);
      } else {
        // Theme and palette changes affect the whole display-settings palette.
        display.drawDisplaySettings(selected, displaySettings.brightness, displaySettings.theme,
          displaySettings.dimTimeout, activeAccent, activeBackground, activeText);
      }
      continue;
    }
    menu.handleEvent(event);

    // ENTER on O2 AIR or O2 PURE starts the corresponding calibration.
    // Check the screen before handling the event so entering the calibration
    // submenu from the main menu does not start a calibration immediately.
    if (screenBeforeEvent == SCREEN_CALIBRATION_MENU &&
        event.button == BTN_ENTER &&
        event.type == BTN_EVENT_SHORT) {

      if (menu.getSelectedItem() == 0) {
        startOxygenCalibration(O2_CAL_POINT_AIR);
      } else if (menu.getSelectedItem() == 1) {
        const bool airCalibrationFresh =
          o2AirCalibratedThisBoot &&
          (uint32_t)(millis() - o2AirCalibrationTime_ms) <= O2_AIR_TO_PURE_WINDOW_MS;

        if (calibration.isOxygenCalibrated() && airCalibrationFresh) {
          startOxygenCalibration(O2_CAL_POINT_PURE);
        } else {
          Serial.println("O2 PURE calibration blocked: AIR calibration required within the last 5 minutes of this boot session.");
          display.drawOxygenCalibrationBlocked("PURE 100 %", "AIR calibration required");
          o2CalibrationPoint = O2_CAL_POINT_PURE;
          o2CalibrationState = O2_CAL_BLOCKED;
        }
      } else if (menu.getSelectedItem() == 2) {
        startHeliumCalibration(HE_CAL_POINT_ZERO);
      } else if (menu.getSelectedItem() == 3) {
        const bool zeroCalibrationFresh =
          heZeroCalibratedThisBoot &&
          (uint32_t)(millis() - heZeroCalibrationTime_ms) <= HE_ZERO_TO_PURE_WINDOW_MS;

        if (calibration.getData().heZeroCalibrated && zeroCalibrationFresh) {
          startHeliumCalibration(HE_CAL_POINT_PURE);
        } else {
          Serial.println("He PURE calibration blocked: ZERO calibration required within the last 5 minutes of this boot session.");
          display.drawHeliumCalibrationBlocked("PURE 100 %", "ZERO calibration required");
          heCalibrationPoint = HE_CAL_POINT_PURE;
          heCalibrationState = HE_CAL_BLOCKED;
        }
      }
    }

    if (event.type == BTN_EVENT_PRESSED) {
      Serial.printf("%s pressed\n", buttonName);
    }

    if (event.type == BTN_EVENT_SHORT) {
      Serial.printf("%s short press\n", buttonName);
    }

    if (event.type == BTN_EVENT_LONG) {
      Serial.printf("%s long press\n", buttonName);
    }
  }

  // Refresh the Wi-Fi password screen when asynchronous connection state changes.
  if (menu.getScreen() == SCREEN_WIFI_PASSWORD) {
    static WifiManager::ConnectState lastWifiConnectState = WifiManager::CONNECT_IDLE;
    WifiManager::ConnectState currentWifiConnectState = wifiManager.getConnectState();
    if (currentWifiConnectState != lastWifiConnectState) {
      lastWifiConnectState = currentWifiConnectState;
      String masked = getMaskedWifiPassword();
      display.drawWifiPassword(wifiManager.getSelectedSSID(), masked,
        wifiPasswordCharIndex, wifiKeyboardSymbols, wifiKeyboardUppercase,
        currentWifiConnectState == WifiManager::CONNECTING,
        currentWifiConnectState == WifiManager::CONNECTED,
        currentWifiConnectState == WifiManager::CONNECT_FAILED);
    }
  }

  // Calibration sampling is checked on every pass; the function itself
  // takes a sample only when its 500 ms deadline has arrived.
  updateOxygenCalibration();
  updateHeliumCalibration();

  uint32_t now = millis();

  // Read sensors periodically instead of reading them on every loop pass.
  // Routine ADC reads are paused while calibration samples are being taken.
  if (o2CalibrationState != O2_CAL_CAPTURING &&
      heCalibrationState != HE_CAL_CAPTURING &&
      (uint32_t)(now - lastSensorUpdate_ms) >= SENSOR_UPDATE_INTERVAL_MS) {

    lastSensorUpdate_ms = now;

    OxygenData o2 = oxygen.update();
    HeliumData he = helium.update();

    if (o2.measurementValid) {
      latestFilteredO2 = oxygenFilter.update(o2.oxygenPercent);
    } else {
      latestFilteredO2 = NAN;
    }

    if (he.voltageValid) {
      float filteredHeVoltage = heliumFilter.update(he.voltage_mV);
      latestFilteredHePercent = helium.calculateHelium(filteredHeVoltage);
    } else {
      latestFilteredHePercent = NAN;
    }

    // Update live gas values only when the main screen is visible.
    if (menu.getScreen() == SCREEN_MAIN) {
      display.updateOxygen(latestFilteredO2);
      display.updateHelium(latestFilteredHePercent);
    }

    // Keep raw sensor voltages visible and refreshed on the Sensor Status screen.
    if (menu.getScreen() == SCREEN_SENSOR_STATUS) {
      display.updateSensorVoltageLine("O2", o2.voltage_mV,
                                      isfinite(o2.voltage_mV), 126);
      display.updateSensorVoltageLine("He", he.voltage_mV,
                                      isfinite(he.voltage_mV), 181);
    }
  }

  // Redraw screen when navigation changes.
  if (menu.hasScreenChanged()) {

    switch (menu.getScreen()) {

      case SCREEN_MAIN:
        display.drawMainScreen(adc.isReady());
        updateBatteryStatus(true);
        break;

      case SCREEN_MAIN_MENU:
        display.drawMainMenu(menu.getSelectedItem());
        break;

      case SCREEN_CALIBRATION_MENU:
        display.drawCalibrationMenu(menu.getSelectedItem());
        break;

      case SCREEN_SETTINGS_MENU:
        display.drawSettingsMenu(menu.getSelectedItem());
        break;

      case SCREEN_DISPLAY_SETTINGS:
        display.drawDisplaySettings(menu.getSelectedItem(), displaySettings.brightness,
                                    displaySettings.theme, displaySettings.dimTimeout,
                                    displaySettings.theme ? displaySettings.lightAccent : displaySettings.darkAccent,
                                    displaySettings.theme ? displaySettings.lightBackground : displaySettings.darkBackground,
                                    displaySettings.theme ? displaySettings.lightText : displaySettings.darkText);
        break;

      case SCREEN_SYSTEM_SETTINGS:
        display.drawSystemSettingsMenu(menu.getSelectedItem(), wifiManager.isEnabled(), wifiManager.isConnected(), wifiManager.isBusy());
        break;

      case SCREEN_WIFI_NETWORKS:
        // Keep existing scan results when they are already available.
        if (wifiManager.getState() == WifiManager::WIFI_IDLE ||
            wifiManager.getState() == WifiManager::WIFI_FAILED) {
          wifiManager.beginScan();
          display.drawWifiNetworks(0, 0, true, false, nullptr, nullptr);
        }
        break;

      case SCREEN_WIFI_PASSWORD: {
        String masked = getMaskedWifiPassword();
        display.drawWifiPassword(wifiManager.getSelectedSSID(), masked,
          wifiPasswordCharIndex, wifiKeyboardSymbols, wifiKeyboardUppercase,
          wifiManager.getConnectState() == WifiManager::CONNECTING,
          wifiManager.getConnectState() == WifiManager::CONNECTED,
          wifiManager.getConnectState() == WifiManager::CONNECT_FAILED);
        break;
      }

      case SCREEN_FIRMWARE_UPDATE:
        firmwareUpdate.reset();
        display.drawFirmwareUpdateInfo(false, false, "", "Press ENTER to check for updates");
        break;

      case SCREEN_SENSOR_STATUS:
        display.drawSensorStatus(
          adc.isReady(),
          oxygen.isReady(),
          helium.isReady(),
          oxygen.getVoltage(), isfinite(oxygen.getVoltage()),
          helium.getVoltage(), isfinite(helium.getVoltage())
        );
        break;

      case SCREEN_ABOUT:
        display.drawAbout();
        break;
    }

    menu.clearScreenChanged();
    menu.clearSelectionChanged();

    // Restore the most recently measured O2 value after returning to main.
    if (menu.getScreen() == SCREEN_MAIN) {
      lastDisplayActivity_ms = millis();
      displayDimmed = false;
      display.setDimmed(false);
      if (isfinite(latestFilteredO2)) display.updateOxygen(latestFilteredO2);
      if (isfinite(latestFilteredHePercent)) display.updateHelium(latestFilteredHePercent);
    }
  }
  else if (menu.hasSelectionChanged()) {

    uint8_t oldItem = menu.getPreviousSelectedItem();
    uint8_t newItem = menu.getSelectedItem();

    switch (menu.getScreen()) {
      case SCREEN_MAIN_MENU:
        display.updateMainMenuSelection(oldItem, newItem);
        break;

      case SCREEN_CALIBRATION_MENU:
        display.updateCalibrationMenuSelection(oldItem, newItem);
        break;

      case SCREEN_SETTINGS_MENU:
        display.updateSettingsMenuSelection(oldItem, newItem);
        break;

      case SCREEN_DISPLAY_SETTINGS:
        display.updateDisplaySettingsSelection(oldItem, newItem, displaySettings.brightness,
                                               displaySettings.theme, displaySettings.dimTimeout,
                                               displaySettings.theme ? displaySettings.lightAccent : displaySettings.darkAccent,
                                               displaySettings.theme ? displaySettings.lightBackground : displaySettings.darkBackground,
                                               displaySettings.theme ? displaySettings.lightText : displaySettings.darkText);
        break;

      case SCREEN_SYSTEM_SETTINGS:
        display.updateSystemSettingsSelection(oldItem, newItem, wifiManager.isEnabled(), wifiManager.isConnected(), wifiManager.isBusy());
        break;

      case SCREEN_WIFI_NETWORKS:
        if (wifiManager.getState() == WifiManager::WIFI_READY) {
          String names[WifiManager::MAX_NETWORKS];
          int32_t signals[WifiManager::MAX_NETWORKS];
          for (uint8_t i = 0; i < wifiManager.getCount(); ++i) {
            names[i] = wifiManager.getSSID(i);
            signals[i] = wifiManager.getRSSI(i);
          }
          display.updateWifiNetworksSelection(oldItem, newItem,
            wifiManager.getCount(), names, signals);
        }
        break;

      default:
        break;
    }

    menu.clearSelectionChanged();
  }

  // Dim only the main measurement screen; ADC polling continues unchanged.
  if (menu.getScreen() == SCREEN_MAIN && !displayDimmed &&
      (uint32_t)(millis() - lastDisplayActivity_ms) >= DIM_TIMEOUTS_MS[displaySettings.dimTimeout]) {
    displayDimmed = true;
    display.setDimmed(true);
  }
  if (menu.getScreen() != SCREEN_MAIN && displayDimmed) {
    displayDimmed = false;
    display.setDimmed(false);
  }

  // Let the ESP32 background tasks run without blocking for a fixed delay.
  yield();
}
