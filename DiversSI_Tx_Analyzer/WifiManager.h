#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>

class WifiManager {
public:
  static const uint8_t MAX_NETWORKS = 12;
  enum ScanState : uint8_t { WIFI_IDLE, WIFI_SCANNING, WIFI_READY, WIFI_FAILED };
  enum ConnectState : uint8_t { CONNECT_IDLE, CONNECTING, CONNECTED, CONNECT_FAILED };

private:
  ScanState state = WIFI_IDLE;
  String names[MAX_NETWORKS];
  int32_t signals[MAX_NETWORKS];
  bool encrypted[MAX_NETWORKS];
  uint8_t count = 0;
  ConnectState connectState = CONNECT_IDLE;
  uint32_t connectStarted_ms = 0;
  String selectedSSID;
  String pendingPassword;
  String savedSSID;
  String savedPassword;
  Preferences prefs;
  bool prefsReady = false;
  bool enabled = false;
  int lastLoggedWiFiStatus = -999;
  bool scanStartPending = false;
  uint32_t scanStartAt_ms = 0;
  uint8_t scanAttempts = 0;
  static const uint8_t MAX_SCAN_ATTEMPTS = 3;
  static const uint32_t SCAN_START_DELAY_MS = 500;

  static const char* scanStateName(ScanState value) {
    switch (value) {
      case WIFI_IDLE: return "IDLE";
      case WIFI_SCANNING: return "SCANNING";
      case WIFI_READY: return "READY";
      case WIFI_FAILED: return "FAILED";
    }
    return "UNKNOWN";
  }

  static const char* connectStateName(ConnectState value) {
    switch (value) {
      case CONNECT_IDLE: return "IDLE";
      case CONNECTING: return "CONNECTING";
      case CONNECTED: return "CONNECTED";
      case CONNECT_FAILED: return "FAILED";
    }
    return "UNKNOWN";
  }

  void loadCredentials() {
    if (!prefsReady) return;
    savedSSID = prefs.getString("ssid", "");
    savedPassword = prefs.getString("pass", "");
    enabled = prefs.getBool("enabled", false);
  }

  void saveCredentials() {
    if (!prefsReady) return;
    prefs.putString("ssid", savedSSID);
    prefs.putString("pass", savedPassword);
  }

public:
  void begin() {
    Serial.println("\n[WiFi-DIAG] ===== Wi-Fi diagnostics started =====");
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
      Serial.printf("[WiFi-DIAG][EVENT] t=%lu ms event=%d\n",
                    (unsigned long)millis(), (int)event);
      if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
        Serial.printf("[WiFi-DIAG][EVENT] STA disconnected, reason=%u\n",
                      (unsigned int)info.wifi_sta_disconnected.reason);
      } else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
        Serial.printf("[WiFi-DIAG][EVENT] GOT_IP localIP=%s\n",
                      WiFi.localIP().toString().c_str());
      }
    });

    prefsReady = prefs.begin("wifi_cfg", false);
    Serial.printf("[WiFi-DIAG][BOOT] Preferences begin=%s\n", prefsReady ? "OK" : "FAILED");
    loadCredentials();
    Serial.printf("[WiFi-DIAG][BOOT] saved enabled=%s, saved SSID='%s', password stored=%s\n",
                  enabled ? "true" : "false", savedSSID.c_str(),
                  savedPassword.length() ? "yes" : "no");
    // Wi-Fi is intentionally OFF after every boot. It is only needed on demand
    // for firmware updates; saved credentials remain available for manual use.
    enabled = false;
    if (prefsReady) prefs.putBool("enabled", false);
    state = WIFI_IDLE;
    connectState = CONNECT_IDLE;
    Serial.printf("[WiFi-DIAG][BOOT] forcing Wi-Fi OFF; saved credentials retained; t=%lu ms\n",
                  (unsigned long)millis());
    WiFi.disconnect(true, false);
    WiFi.mode(WIFI_OFF);
    Serial.printf("[WiFi-DIAG][BOOT] WiFi.mode(WIFI_OFF), status=%d\n", (int)WiFi.status());
  }

  void setEnabled(bool on) {
    Serial.printf("[WiFi-DIAG][TOGGLE] request=%s at t=%lu ms; previous state=%s; connect=%s\n",
                  on ? "ON" : "OFF", (unsigned long)millis(),
                  scanStateName(state), connectStateName(connectState));
    enabled = on;
    if (prefsReady) prefs.putBool("enabled", enabled);
    if (!on) {
      WiFi.disconnect(true, false);
      WiFi.mode(WIFI_OFF);
      state = WIFI_IDLE;
      connectState = CONNECT_IDLE;
      scanStartPending = false;
      scanAttempts = 0;
      count = 0;
      Serial.printf("[WiFi-DIAG][TOGGLE] OFF complete; WiFi.status=%d\n", (int)WiFi.status());
      return;
    }
    bool modeOk = WiFi.mode(WIFI_STA);
    Serial.printf("[WiFi-DIAG][TOGGLE] WIFI_STA requested, mode result=%s, status=%d\n",
                  modeOk ? "true" : "false", (int)WiFi.status());
    connectState = CONNECT_IDLE;
    scanAttempts = 0;
    beginScan();
  }

  bool isEnabled() const { return enabled; }
  bool isConnected() const { return enabled && WiFi.status() == WL_CONNECTED; }
  bool isBusy() const {
    return enabled && (state == WIFI_SCANNING || connectState == CONNECTING);
  }
  bool hasSavedNetwork() const { return savedSSID.length() > 0; }

  void beginScan() {
    if (!enabled) {
      Serial.println("[WiFi-DIAG][SCAN] ignored: Wi-Fi disabled");
      return;
    }
    if (state == WIFI_SCANNING || scanStartPending) {
      Serial.println("[WiFi-DIAG][SCAN] ignored: scan already running or scheduled");
      return;
    }
    if (scanAttempts >= MAX_SCAN_ATTEMPTS) scanAttempts = 0;
    scanStartPending = true;
    scanStartAt_ms = millis() + SCAN_START_DELAY_MS;
    state = WIFI_IDLE;
    Serial.printf("[WiFi-DIAG][SCAN] scheduled attempt %u/%u in %lu ms; WiFi.status=%d\n",
                  (unsigned int)(scanAttempts + 1), (unsigned int)MAX_SCAN_ATTEMPTS,
                  (unsigned long)SCAN_START_DELAY_MS, (int)WiFi.status());
  }

  void startScheduledScan() {
    scanStartPending = false;
    ++scanAttempts;
    Serial.printf("[WiFi-DIAG][SCAN] starting attempt %u/%u at t=%lu ms\n",
                  (unsigned int)scanAttempts, (unsigned int)MAX_SCAN_ATTEMPTS,
                  (unsigned long)millis());
    bool modeOk = WiFi.mode(WIFI_STA);
    Serial.printf("[WiFi-DIAG][SCAN] set WIFI_STA result=%s; status=%d\n",
                  modeOk ? "true" : "false", (int)WiFi.status());
    WiFi.scanDelete();
    count = 0;
    state = WIFI_SCANNING;
    int16_t result = WiFi.scanNetworks(true, true);
    Serial.printf("[WiFi-DIAG][SCAN] scanNetworks(async=true, show_hidden=true) returned %d at t=%lu ms\n",
                  (int)result, (unsigned long)millis());
    if (result == WIFI_SCAN_FAILED) {
      state = WIFI_FAILED;
      Serial.printf("[WiFi-DIAG][SCAN] immediate FAILURE; WiFi.status=%d\n", (int)WiFi.status());
    }
  }

  void update() {
    int currentWiFiStatus = (int)WiFi.status();
    if (currentWiFiStatus != lastLoggedWiFiStatus) {
      Serial.printf("[WiFi-DIAG][STATUS] t=%lu ms WiFi.status=%d scan=%s connect=%s enabled=%s\n",
                    (unsigned long)millis(), currentWiFiStatus,
                    scanStateName(state), connectStateName(connectState), enabled ? "true" : "false");
      lastLoggedWiFiStatus = currentWiFiStatus;
    }

    if (scanStartPending && (int32_t)(millis() - scanStartAt_ms) >= 0) {
      startScheduledScan();
    }

    if (connectState == CONNECTING) {
      if (WiFi.status() == WL_CONNECTED) {
        connectState = CONNECTED;
        savedSSID = selectedSSID;
        savedPassword = pendingPassword;
        saveCredentials();
        Serial.printf("[WiFi-DIAG][CONNECT] SUCCESS SSID='%s' IP=%s elapsed=%lu ms; credentials saved\n",
                      selectedSSID.c_str(), WiFi.localIP().toString().c_str(),
                      (unsigned long)(millis() - connectStarted_ms));
      } else if ((uint32_t)(millis() - connectStarted_ms) >= 20000UL) {
        Serial.printf("[WiFi-DIAG][CONNECT] TIMEOUT SSID='%s' after 20000 ms; status=%d\n",
                      selectedSSID.c_str(), (int)WiFi.status());
        WiFi.disconnect();
        connectState = CONNECT_FAILED;
      }
    }
    if (state != WIFI_SCANNING) return;
    int16_t result = WiFi.scanComplete();
    if (result == WIFI_SCAN_RUNNING) return;
    Serial.printf("[WiFi-DIAG][SCAN] scanComplete returned %d at t=%lu ms\n",
                  (int)result, (unsigned long)millis());
    if (result == WIFI_SCAN_FAILED) {
      state = WIFI_FAILED;
      Serial.printf("[WiFi-DIAG][SCAN] FAILED after %lu ms; WiFi.status=%d\n",
                    (unsigned long)millis(), (int)WiFi.status());
      if (scanAttempts < MAX_SCAN_ATTEMPTS) {
        scanStartPending = true;
        scanStartAt_ms = millis() + SCAN_START_DELAY_MS;
        Serial.printf("[WiFi-DIAG][SCAN] retry scheduled in %lu ms (%u/%u)\n",
                      (unsigned long)SCAN_START_DELAY_MS,
                      (unsigned int)(scanAttempts + 1), (unsigned int)MAX_SCAN_ATTEMPTS);
      } else {
        Serial.println("[WiFi-DIAG][SCAN] retry limit reached; waiting for user action");
      }
      return;
    }
    count = (result > MAX_NETWORKS) ? MAX_NETWORKS : (uint8_t)result;
    Serial.printf("[WiFi-DIAG][SCAN] completed: total=%d, stored=%u\n", (int)result, count);
    for (uint8_t i = 0; i < count; ++i) {
      names[i] = WiFi.SSID(i);
      signals[i] = WiFi.RSSI(i);
      encrypted[i] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
      Serial.printf("[WiFi-DIAG][SCAN] network %u: SSID='%s', RSSI=%ld dBm, encrypted=%s\n",
                    (unsigned int)i, names[i].c_str(), (long)signals[i],
                    encrypted[i] ? "yes" : "no");
    }
    WiFi.scanDelete();
    state = WIFI_READY;
    scanAttempts = 0;
    Serial.printf("[WiFi-DIAG][SCAN] state -> READY, count=%u\n", count);
  }

  bool connectSavedIfFound() {
    Serial.printf("[WiFi-DIAG][AUTO] check saved network; enabled=%s savedSSID='%s' scanState=%s count=%u\n",
                  enabled ? "true" : "false", savedSSID.c_str(), scanStateName(state), count);
    if (!enabled || !savedSSID.length() || state != WIFI_READY) {
      Serial.println("[WiFi-DIAG][AUTO] skipped: prerequisites not met");
      return false;
    }
    int best = -1;
    for (uint8_t i = 0; i < count; ++i) {
      if (names[i] == savedSSID && (best < 0 || signals[i] > signals[best])) best = i;
    }
    if (best < 0) {
      Serial.printf("[WiFi-DIAG][AUTO] saved SSID '%s' NOT FOUND in %u scan results\n",
                    savedSSID.c_str(), count);
      return false;
    }
    Serial.printf("[WiFi-DIAG][AUTO] saved SSID found at index=%d RSSI=%ld dBm; starting auto-connect\n",
                  best, (long)signals[best]);
    selectNetwork((uint8_t)best);
    return connectSelected(savedPassword);
  }

  ScanState getState() const { return state; }
  uint8_t getCount() const { return count; }
  const String& getSSID(uint8_t index) const { return names[index < count ? index : 0]; }
  int32_t getRSSI(uint8_t index) const { return signals[index < count ? index : 0]; }
  bool isEncrypted(uint8_t index) const { return index < count && encrypted[index]; }

  bool selectNetwork(uint8_t index) {
    if (index >= count) {
      Serial.printf("[WiFi-DIAG][SELECT] invalid index=%u count=%u\n", index, count);
      return false;
    }
    selectedIndex = index;
    selectedSSID = names[index];
    connectState = CONNECT_IDLE;
    Serial.printf("[WiFi-DIAG][SELECT] index=%u SSID='%s' RSSI=%ld dBm encrypted=%s\n",
                  index, selectedSSID.c_str(), (long)signals[index], encrypted[index] ? "yes" : "no");
    return true;
  }

  bool connectSelected(const String& password) {
    if (!enabled || selectedSSID.length() == 0) {
      Serial.printf("[WiFi-DIAG][CONNECT] rejected: enabled=%s SSID length=%u\n",
                    enabled ? "true" : "false", (unsigned int)selectedSSID.length());
      return false;
    }
    pendingPassword = password;
    connectState = CONNECTING;
    connectStarted_ms = millis();
    Serial.printf("[WiFi-DIAG][CONNECT] begin SSID='%s' passwordLength=%u at t=%lu ms; previous status=%d\n",
                  selectedSSID.c_str(), (unsigned int)password.length(),
                  (unsigned long)connectStarted_ms, (int)WiFi.status());
    bool modeOk = WiFi.mode(WIFI_STA);
    Serial.printf("[WiFi-DIAG][CONNECT] set WIFI_STA=%s\n", modeOk ? "true" : "false");
    WiFi.disconnect();
    Serial.printf("[WiFi-DIAG][CONNECT] after disconnect status=%d; calling WiFi.begin()\n", (int)WiFi.status());
    wl_status_t beginResult = WiFi.begin(selectedSSID.c_str(), password.c_str());
    Serial.printf("[WiFi-DIAG][CONNECT] WiFi.begin returned=%d; current status=%d\n",
                  (int)beginResult, (int)WiFi.status());
    return true;
  }

  bool connectTo(uint8_t index, const String& password) {
    return selectNetwork(index) && connectSelected(password);
  }
  void clearConnectResult() {
    if (connectState == CONNECT_FAILED) {
      Serial.println("[WiFi-DIAG][CONNECT] clearing FAILED result");
      connectState = CONNECT_IDLE;
    }
  }
  void cancelConnection() {
    Serial.printf("[WiFi-DIAG][CONNECT] cancel requested; state=%s\n", connectStateName(connectState));
    if (connectState == CONNECTING) WiFi.disconnect();
    connectState = CONNECT_IDLE;
  }
  ConnectState getConnectState() const { return connectState; }
  const String& getSelectedSSID() const { return selectedSSID; }

private:
  uint8_t selectedIndex = 0;
};

#endif
