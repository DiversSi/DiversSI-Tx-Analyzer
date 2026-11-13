#ifndef FIRMWARE_UPDATE_MANAGER_H
#define FIRMWARE_UPDATE_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <mbedtls/sha256.h>
#include <time.h>
#include "GitHubRootCAs.h"

// Public metadata file in the project's GitHub repository.
static const char FIRMWARE_VERSION_JSON_URL[] =
  "https://raw.githubusercontent.com/DiversSi/DiversSI-Tx-Analyzer/main/version.json";

class FirmwareUpdateManager {
private:
  String latestVersion;
  String firmwareUrl;
  String firmwareSha256;
  String releaseUrl;
  String statusMessage = "Press ENTER to check for updates";
  bool checked = false;
  bool successful = false;
  bool packageMetadataValid = false;
  bool readyToInstall = false;
  // -1: remote is older, 0: equal, +1: remote is newer.
  int versionComparison = 0;

  static String trimLeadingZeros(String value) {
    int i = 0;
    while (i < (int)value.length() - 1 && value[i] == '0') ++i;
    return value.substring(i);
  }

  static bool isNumericIdentifier(const String& value) {
    if (value.length() == 0) return false;
    for (int i = 0; i < (int)value.length(); ++i) {
      if (value[i] < '0' || value[i] > '9') return false;
    }
    return true;
  }

  static int compareNumericStrings(const String& a, const String& b) {
    String aa = trimLeadingZeros(a);
    String bb = trimLeadingZeros(b);
    if (aa.length() < bb.length()) return -1;
    if (aa.length() > bb.length()) return 1;
    if (aa == bb) return 0;
    return aa < bb ? -1 : 1;
  }

  static bool splitVersion(const String& input, String& core, String& prerelease) {
    String value = input;
    value.trim();
    if (value.startsWith("v") || value.startsWith("V")) value.remove(0, 1);
    int plus = value.indexOf('+');
    if (plus >= 0) value = value.substring(0, plus);
    int dash = value.indexOf('-');
    core = (dash >= 0) ? value.substring(0, dash) : value;
    prerelease = (dash >= 0) ? value.substring(dash + 1) : "";
    if (core.length() == 0) return false;

    uint8_t coreParts = 0;
    int start = 0;
    while (start <= (int)core.length()) {
      int dot = core.indexOf('.', start);
      String part = (dot < 0) ? core.substring(start) : core.substring(start, dot);
      if (!isNumericIdentifier(part)) return false;
      ++coreParts;
      if (dot < 0) break;
      start = dot + 1;
    }
    if (coreParts < 2 || coreParts > 3) return false;

    if (prerelease.length() > 0) {
      start = 0;
      while (start <= (int)prerelease.length()) {
        int dot = prerelease.indexOf('.', start);
        String part = (dot < 0) ? prerelease.substring(start) : prerelease.substring(start, dot);
        if (part.length() == 0) return false;
        for (int i = 0; i < (int)part.length(); ++i) {
          char c = part[i];
          if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
                (c >= 'a' && c <= 'z') || c == '-')) return false;
        }
        if (dot < 0) break;
        start = dot + 1;
      }
    }
    return true;
  }

  static int compareTextIdentifiers(const String& a, const String& b) {
    int aDigit = (int)a.length();
    int bDigit = (int)b.length();
    while (aDigit > 0 && a[aDigit - 1] >= '0' && a[aDigit - 1] <= '9') --aDigit;
    while (bDigit > 0 && b[bDigit - 1] >= '0' && b[bDigit - 1] <= '9') --bDigit;
    String aPrefix = a.substring(0, aDigit);
    String bPrefix = b.substring(0, bDigit);
    if (aDigit < (int)a.length() && bDigit < (int)b.length() && aPrefix == bPrefix) {
      return compareNumericStrings(a.substring(aDigit), b.substring(bDigit));
    }
    if (a == b) return 0;
    return a < b ? -1 : 1;
  }

  static int comparePrerelease(const String& a, const String& b) {
    if (a.length() == 0 && b.length() == 0) return 0;
    if (a.length() == 0) return 1;
    if (b.length() == 0) return -1;

    int startA = 0, startB = 0;
    while (true) {
      int dotA = a.indexOf('.', startA);
      int dotB = b.indexOf('.', startB);
      String partA = (dotA < 0) ? a.substring(startA) : a.substring(startA, dotA);
      String partB = (dotB < 0) ? b.substring(startB) : b.substring(startB, dotB);
      bool numA = isNumericIdentifier(partA);
      bool numB = isNumericIdentifier(partB);
      int result = 0;
      if (numA && numB) result = compareNumericStrings(partA, partB);
      else if (numA != numB) result = numA ? -1 : 1;
      else if (partA != partB) result = compareTextIdentifiers(partA, partB);
      if (result != 0) return result;

      bool endA = dotA < 0;
      bool endB = dotB < 0;
      if (endA && endB) return 0;
      if (endA) return -1;
      if (endB) return 1;
      startA = dotA + 1;
      startB = dotB + 1;
    }
  }

  static bool compareVersions(const String& installed, const String& available, int& result) {
    String installedCore, installedPre, availableCore, availablePre;
    if (!splitVersion(installed, installedCore, installedPre) ||
        !splitVersion(available, availableCore, availablePre)) return false;

    int startA = 0, startB = 0;
    for (uint8_t part = 0; part < 3; ++part) {
      int dotA = installedCore.indexOf('.', startA);
      int dotB = availableCore.indexOf('.', startB);
      String a = (startA >= (int)installedCore.length()) ? "0" :
        ((dotA < 0) ? installedCore.substring(startA) : installedCore.substring(startA, dotA));
      String b = (startB >= (int)availableCore.length()) ? "0" :
        ((dotB < 0) ? availableCore.substring(startB) : availableCore.substring(startB, dotB));
      int coreResult = compareNumericStrings(b, a);
      if (coreResult != 0) { result = coreResult; return true; }
      if (dotA >= 0) startA = dotA + 1; else startA = installedCore.length();
      if (dotB >= 0) startB = dotB + 1; else startB = availableCore.length();
    }

    result = comparePrerelease(availablePre, installedPre);
    return true;
  }

  static bool extractJsonString(const String& json, const char* key, String& value) {
    String token = String("\"") + key + "\"";
    int keyPos = json.indexOf(token);
    if (keyPos < 0) return false;
    int colonPos = json.indexOf(':', keyPos + token.length());
    if (colonPos < 0) return false;
    int quotePos = json.indexOf('"', colonPos + 1);
    if (quotePos < 0) return false;

    value = "";
    bool escaped = false;
    for (int i = quotePos + 1; i < (int)json.length(); ++i) {
      char c = json[i];
      if (escaped) {
        if (c == '"' || c == '\\' || c == '/') value += c;
        else if (c == 'n') value += '\n';
        else if (c == 'r') value += '\r';
        else if (c == 't') value += '\t';
        else value += c;
        escaped = false;
      } else if (c == '\\') {
        escaped = true;
      } else if (c == '"') {
        return true;
      } else {
        value += c;
      }
    }
    return false;
  }

  static bool isValidSha256(const String& value) {
    if (value.length() != 64) return false;
    for (int i = 0; i < 64; ++i) {
      char c = value[i];
      if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
            (c >= 'A' && c <= 'F'))) return false;
    }
    return true;
  }

  static bool isHttpsUrl(const String& value) {
    return value.startsWith("https://") && value.length() > 8;
  }

  static void makeSha256Hex(const uint8_t digest[32], char output[65]) {
    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < 32; ++i) {
      output[i * 2] = hex[(digest[i] >> 4) & 0x0F];
      output[i * 2 + 1] = hex[digest[i] & 0x0F];
    }
    output[64] = '\0';
  }

  static bool sha256Matches(const uint8_t digest[32], const String& expected) {
    char actual[65];
    makeSha256Hex(digest, actual);
    String wanted = expected;
    wanted.toLowerCase();
    return wanted == String(actual);
  }

  bool ensureSystemTime() {
    // TLS certificate validity checks require a reasonably accurate clock.
    time_t now = time(nullptr);
    if (now > 1700000000) return true;

    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    const unsigned long startedAt = millis();
    while (millis() - startedAt < 15000UL) {
      now = time(nullptr);
      if (now > 1700000000) return true;
      delay(250);
      yield();
    }
    return false;
  }

  bool configureSecureClient(WiFiClientSecure& client) {
    client.setCACert(GITHUB_ROOT_CA_BUNDLE);
    client.setTimeout(15000);
    return true;
  }

public:
  void reset() {
    latestVersion = "";
    firmwareUrl = "";
    firmwareSha256 = "";
    releaseUrl = "";
    checked = false;
    successful = false;
    packageMetadataValid = false;
    readyToInstall = false;
    versionComparison = 0;
    statusMessage = "Press ENTER to check for updates";
  }

  bool checkLatest(bool wifiConnected) {
    checked = true;
    successful = false;
    readyToInstall = false;
    latestVersion = "";
    firmwareUrl = "";
    firmwareSha256 = "";
    releaseUrl = "";
    packageMetadataValid = false;
    versionComparison = 0;

    if (!wifiConnected || WiFi.status() != WL_CONNECTED) {
      statusMessage = "Wi-Fi is not connected";
      return false;
    }

    if (!ensureSystemTime()) {
      statusMessage = "NTP time sync failed - HTTPS unavailable";
      return false;
    }

    WiFiClientSecure client;
    configureSecureClient(client);

    HTTPClient http;
    http.setTimeout(12000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    if (!http.begin(client, FIRMWARE_VERSION_JSON_URL)) {
      statusMessage = "Could not start secure request";
      return false;
    }

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
      char tlsError[160] = {0};
      int tlsErrorCode = client.lastError(tlsError, sizeof(tlsError));
      statusMessage = String("HTTPS ") + httpCode +
        " TLS " + tlsErrorCode + ": " + String(tlsError);
      http.end();
      return false;
    }

    String body = http.getString();
    http.end();

    if (!extractJsonString(body, "version", latestVersion) || latestVersion.length() == 0) {
      statusMessage = "Invalid version.json";
      latestVersion = "";
      return false;
    }

    if (!compareVersions(PROJECT_VERSION, latestVersion, versionComparison)) {
      statusMessage = "Invalid version format";
      latestVersion = "";
      return false;
    }

    extractJsonString(body, "release_url", releaseUrl);
    bool hasFirmwareUrl = extractJsonString(body, "firmware_url", firmwareUrl);
    bool hasSha256 = extractJsonString(body, "sha256", firmwareSha256);
    packageMetadataValid = hasFirmwareUrl && hasSha256 &&
      isHttpsUrl(firmwareUrl) && isValidSha256(firmwareSha256);

    successful = true;
    if (versionComparison > 0) {
      if (packageMetadataValid) {
        readyToInstall = true;
        statusMessage = "Update available - ENTER to install";
      } else {
        statusMessage = "Update metadata incomplete";
      }
    } else if (versionComparison == 0) {
      statusMessage = "Firmware is up to date";
    } else {
      statusMessage = "Installed version is newer";
    }
    return true;
  }

  // Call only after the user presses ENTER on the explicit install prompt.
  bool installUpdate(bool wifiConnected) {
    if (!readyToInstall || !packageMetadataValid) {
      statusMessage = "No verified update package";
      return false;
    }
    if (!wifiConnected || WiFi.status() != WL_CONNECTED) {
      statusMessage = "Wi-Fi is not connected";
      return false;
    }

    statusMessage = "Synchronizing clock";
    if (!ensureSystemTime()) {
      statusMessage = "NTP time sync failed - HTTPS unavailable";
      return false;
    }

    statusMessage = "Connecting to firmware";
    WiFiClientSecure client;
    configureSecureClient(client);

    HTTPClient http;
    http.setTimeout(20000);
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    if (!http.begin(client, firmwareUrl)) {
      statusMessage = "Could not start firmware download";
      return false;
    }

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
      char tlsError[256] = {0};
      int tlsErrorCode = client.lastError(tlsError, sizeof(tlsError));
      String redirectLocation = http.getLocation();

      Serial.println();
      Serial.println(F("[OTA] Firmware download request failed"));
      Serial.printf("[OTA] HTTP code: %d\\n", httpCode);
      Serial.printf("[OTA] TLS error code: %d\\n", tlsErrorCode);
      Serial.printf("[OTA] TLS error text: %s\\n", tlsError);
      Serial.printf("[OTA] Requested URL: %s\\n", firmwareUrl.c_str());
      if (redirectLocation.length() > 0) {
        Serial.printf("[OTA] Redirect location: %s\\n", redirectLocation.c_str());
      } else {
        Serial.println(F("[OTA] Redirect location: <not provided by HTTPClient>"));
      }
      Serial.flush();

      statusMessage = String("Firmware HTTP ") + httpCode +
        " TLS " + tlsErrorCode + ": " + String(tlsError);
      http.end();
      return false;
    }

    int contentLength = http.getSize();
    if (contentLength <= 0) {
      statusMessage = "Invalid firmware file size";
      http.end();
      return false;
    }

    if (!Update.begin((size_t)contentLength, U_FLASH)) {
      statusMessage = String("OTA space error: ") + Update.errorString();
      http.end();
      return false;
    }

    mbedtls_sha256_context shaContext;
    mbedtls_sha256_init(&shaContext);
    if (mbedtls_sha256_starts(&shaContext, 0) != 0) {
      mbedtls_sha256_free(&shaContext);
      Update.abort();
      http.end();
      statusMessage = "SHA-256 initialization failed";
      return false;
    }

    auto* stream = http.getStreamPtr();
    uint8_t buffer[1024];
    size_t totalWritten = 0;
    bool writeOk = true;
    unsigned long lastDataAt = millis();

    while (http.connected() && totalWritten < (size_t)contentLength) {
      size_t availableBytes = stream->available();
      if (availableBytes) {
        int bytesToRead = (availableBytes > sizeof(buffer)) ? sizeof(buffer) : (int)availableBytes;
        int bytesRead = stream->readBytes(buffer, bytesToRead);
        if (bytesRead <= 0) break;

        if (mbedtls_sha256_update(&shaContext, buffer, (size_t)bytesRead) != 0) {
          writeOk = false;
          statusMessage = "SHA-256 update failed";
          break;
        }

        size_t written = Update.write(buffer, (size_t)bytesRead);
        if (written != (size_t)bytesRead) {
          writeOk = false;
          statusMessage = "Firmware flash write failed";
          break;
        }
        totalWritten += written;
        lastDataAt = millis();
      } else {
        if (millis() - lastDataAt > 20000UL) {
          writeOk = false;
          statusMessage = "Firmware download timeout";
          break;
        }
        delay(10);
      }
      yield();
    }

    http.end();

    if (totalWritten != (size_t)contentLength) {
      writeOk = false;
      if (statusMessage == "Connecting to firmware") {
        statusMessage = "Firmware download incomplete";
      }
    }

    uint8_t digest[32];
    if (writeOk && mbedtls_sha256_finish(&shaContext, digest) != 0) {
      writeOk = false;
      statusMessage = "SHA-256 finalization failed";
    }
    mbedtls_sha256_free(&shaContext);

    if (!writeOk) {
      Update.abort();
      return false;
    }

    if (!sha256Matches(digest, firmwareSha256)) {
      Update.abort();
      statusMessage = "SHA-256 verification failed";
      return false;
    }

    if (!Update.end()) {
      statusMessage = String("OTA finalize error: ") + Update.errorString();
      return false;
    }

    if (!Update.isFinished()) {
      statusMessage = "OTA image not complete";
      return false;
    }

    statusMessage = "Update installed - restarting";
    delay(500);
    ESP.restart();
    return true;
  }

  bool hasChecked() const { return checked; }
  bool isSuccessful() const { return successful; }
  const String& getLatestVersion() const { return latestVersion; }
  const String& getStatusMessage() const { return statusMessage; }
  const String& getFirmwareUrl() const { return firmwareUrl; }
  const String& getFirmwareSha256() const { return firmwareSha256; }
  const String& getReleaseUrl() const { return releaseUrl; }
  bool isPackageMetadataValid() const { return successful && packageMetadataValid; }
  bool isUpdateAvailable() const { return successful && versionComparison > 0; }
  bool isUpToDate() const { return successful && versionComparison == 0; }
  bool isInstalledNewer() const { return successful && versionComparison < 0; }
  bool isReadyToInstall() const { return readyToInstall; }
};

#endif
