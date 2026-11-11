
#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "Config.h"

class DisplayManager {

  private:

    TFT_eSPI tft = TFT_eSPI();

    // Display colors
    uint16_t COLOR_BG       = TFT_BLACK;
    uint16_t COLOR_HEADER   = TFT_NAVY;
    uint16_t COLOR_O2       = TFT_GREEN;
    uint16_t COLOR_HE       = TFT_CYAN;
    uint16_t COLOR_TEXT     = TFT_WHITE;
    uint16_t COLOR_MUTED    = TFT_LIGHTGREY;
    uint16_t COLOR_OK       = TFT_GREEN;
    uint16_t COLOR_ERROR    = TFT_RED;
    uint16_t COLOR_PANEL    = 0x2104;
    uint8_t brightnessPercent = 100;
    bool lightTheme = false;

    bool displayReady = false;

    void drawHeader() {

      tft.fillRect(0, 0, DISPLAY_WIDTH, 42, COLOR_HEADER);

      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_WHITE, COLOR_HEADER);

      tft.drawString(PROJECT_NAME, 160, 21, 4);
    }

    void drawSensorPanels() {

      // O2 panel
      tft.fillRoundRect(8, 52, 148, 125, COLOR_PANEL, 8);
      // 4-pixel O2 border (TFT_eSPI drawRoundRect has no thickness argument)
      tft.drawRoundRect(8, 52, 148, 125, 8, COLOR_O2);
      tft.drawRoundRect(9, 53, 146, 123, 7, COLOR_O2);
      tft.drawRoundRect(10, 54, 144, 121, 6, COLOR_O2);
      tft.drawRoundRect(11, 55, 142, 119, 5, COLOR_O2);

      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(COLOR_O2, COLOR_PANEL);
      tft.drawString("OXYGEN", 82, 72, 4);

      tft.setTextColor(COLOR_TEXT, COLOR_PANEL);
      tft.drawString("--.- %", 82, 120, 4);

      // Helium panel
      tft.fillRoundRect(164, 52, 148, 125, COLOR_PANEL, 8);
      // 4-pixel helium border
      tft.drawRoundRect(164, 52, 148, 125, 8, COLOR_HE);
      tft.drawRoundRect(165, 53, 146, 123, 7, COLOR_HE);
      tft.drawRoundRect(166, 54, 144, 121, 6, COLOR_HE);
      tft.drawRoundRect(167, 55, 142, 119, 5, COLOR_HE);

      tft.setTextColor(COLOR_HE, COLOR_PANEL);
      tft.drawString("HELIUM", 238, 72, 4);

      tft.setTextColor(COLOR_TEXT, COLOR_PANEL);
      tft.drawString("--.- %", 238, 120, 4);
    }

    void drawStatus(bool adcReady) {

      tft.fillRect(0, 183, DISPLAY_WIDTH, 25, COLOR_BG);

      tft.setTextDatum(ML_DATUM);
      tft.setTextColor(COLOR_MUTED, COLOR_BG);

      tft.drawString("ADS1115:", 12, 188, 2);

      if (adcReady) {
        tft.setTextColor(COLOR_OK, COLOR_BG);
        tft.drawString("OK", 105, 188, 2);
      } else {
        tft.setTextColor(COLOR_ERROR, COLOR_BG);
        tft.drawString("ERROR", 105, 188, 2);
      }

      tft.setTextColor(COLOR_MUTED, COLOR_BG);
      tft.drawString("GAIN 4", 220, 188, 2);
    }

    void drawFooter() {
      tft.fillRect(0, 211, DISPLAY_WIDTH, 29, COLOR_HEADER);

      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_WHITE, COLOR_HEADER);

      tft.drawString("MENU", 267, 225, 2);
    }

  public:

    bool begin() {

      tft.init();
      tft.setRotation(1);
      pinMode(TFT_BL, OUTPUT);
      setBrightness(100);
      tft.fillScreen(COLOR_BG);

      displayReady = true;

      return true;
    }

    bool isReady() {
      return displayReady;
    }

    void setBrightness(uint8_t percent) {
      if (percent < 10) percent = 10;
      if (percent > 100) percent = 100;
      brightnessPercent = percent;
      analogWrite(TFT_BL, (uint32_t)percent * 255U / 100U);
    }

    void setDimmed(bool dimmed) {
      if (dimmed) analogWrite(TFT_BL, 0);
      else setBrightness(brightnessPercent);
    }

    void setTheme(bool light) {
      lightTheme = light;
      if (light) {
        COLOR_BG = TFT_WHITE; COLOR_HEADER = 0xC618; COLOR_PANEL = 0xEF7D;
        COLOR_TEXT = TFT_BLACK; COLOR_MUTED = 0x7BEF;
      } else {
        COLOR_BG = TFT_BLACK; COLOR_HEADER = TFT_NAVY; COLOR_PANEL = 0x2104;
        COLOR_TEXT = TFT_WHITE; COLOR_MUTED = TFT_LIGHTGREY;
      }
      COLOR_O2 = TFT_GREEN; COLOR_HE = TFT_CYAN;
      COLOR_OK = TFT_GREEN; COLOR_ERROR = TFT_RED;
    }

    void drawDisplaySettings(uint8_t selected, uint8_t brightness,
                             uint8_t theme, uint8_t dimTimeout) {
      if (!displayReady) return;
      const char* timeoutLabels[] = {"30 seconds", "1 minute", "2 minutes", "5 minutes"};
      String values[] = {String(brightness) + " %", theme ? "Light" : "Dark", timeoutLabels[dimTimeout]};
      const char* labels[] = {"Brightness", "Color scheme", "Dim timeout"};
      tft.fillScreen(COLOR_BG);
      tft.fillRect(0, 0, DISPLAY_WIDTH, 42, COLOR_HEADER);
      tft.setTextDatum(MC_DATUM); tft.setTextColor(TFT_WHITE, COLOR_HEADER);
      tft.drawString("DISPLAY SETTINGS", 160, 21, 4);
      for (uint8_t i=0; i<3; ++i) {
        int y=55+i*48; uint16_t bg=(i==selected)?COLOR_HEADER:COLOR_PANEL;
        tft.fillRoundRect(12,y,296,40,bg,5);
        tft.setTextDatum(ML_DATUM); tft.setTextColor(COLOR_TEXT,bg);
        tft.drawString(labels[i],22,y+20,2);
        tft.setTextDatum(MR_DATUM); tft.drawString(values[i],298,y+20,2);
      }
      tft.fillRect(0,211,DISPLAY_WIDTH,29,COLOR_HEADER);
      tft.setTextDatum(MC_DATUM); tft.setTextColor(TFT_WHITE,COLOR_HEADER);
      tft.drawString("UP/DOWN - SELECT  ENTER - CHANGE",160,225,2);
    }

    void drawMainScreen(bool adcReady) {

      if (!displayReady) return;

      tft.fillScreen(COLOR_BG);

      drawHeader();
      drawSensorPanels();
      drawStatus(adcReady);
      drawFooter();
    }
    

    void drawCalibrationMenu(uint8_t selectedItem) {

      if (!displayReady) return;

      const char* menuItems[] = {
        "O2 - AIR 20.9 %",
        "O2 - PURE 100 %",
        "He - ZERO 0 %",
        "He - PURE 100 %"
      };

      tft.fillScreen(COLOR_BG);

      // Header
      tft.fillRect(0, 0, DISPLAY_WIDTH, 42, COLOR_HEADER);

      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_WHITE, COLOR_HEADER);
      tft.drawString("CALIBRATION MENU", 160, 21, 4);

      // Menu items
      for (uint8_t i = 0; i < 4; i++) {

        int y = 55 + (i * 38);

        uint16_t bgColor = (i == selectedItem)
                           ? COLOR_HEADER
                           : COLOR_PANEL;

        tft.fillRoundRect(15, y, 290, 32, bgColor, 5);

        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(TFT_WHITE, bgColor);
        tft.drawString(menuItems[i], 30, y + 16, 4);
      }
      
      // Footer
      tft.fillRect(0, 211, DISPLAY_WIDTH, 29, COLOR_HEADER);

      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_WHITE, COLOR_HEADER);

      tft.drawString("UP", 53, 225, 2);
      tft.drawString("DOWN", 160, 225, 2);
      tft.drawString("ENTER", 267, 225, 2);
    }

    void drawMainMenu(uint8_t selectedItem) {
      if (!displayReady) return;

      const char* items[] = {
        "Calibration",
        "Settings",
        "Sensor status",
        "About"
      };

      tft.fillScreen(COLOR_BG);

      tft.fillRect(0, 0, DISPLAY_WIDTH, 42, COLOR_HEADER);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_WHITE, COLOR_HEADER);
      tft.drawString("MAIN MENU", 160, 21, 4);

      for (uint8_t i = 0; i < 4; i++) {
        int y = 55 + i * 38;

        uint16_t bg = (i == selectedItem) ? COLOR_HEADER : COLOR_PANEL;

        tft.fillRoundRect(15, y, 290, 32, bg, 5);

        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(COLOR_TEXT, bg);
        tft.drawString(items[i], 30, y + 16, 4);
      }

      tft.fillRect(0, 211, DISPLAY_WIDTH, 29, COLOR_HEADER);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_WHITE, COLOR_HEADER);
      tft.drawString("UP", 53, 225, 2);
      tft.drawString("DOWN", 160, 225, 2);
      tft.drawString("ENTER", 267, 225, 2);
    }

    void drawSettingsMenu(uint8_t selectedItem) {
      if (!displayReady) return;

      const char* items[] = {
        "Display settings",
        "System settings"
      };

      tft.fillScreen(COLOR_BG);
      tft.fillRect(0, 0, DISPLAY_WIDTH, 42, COLOR_HEADER);

      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_WHITE, COLOR_HEADER);
      tft.drawString("SETTINGS", 160, 21, 4);

      for (uint8_t i = 0; i < 2; i++) {
        int y = 65 + i * 45;

        uint16_t bg = (i == selectedItem) ? COLOR_HEADER : COLOR_PANEL;

        tft.fillRoundRect(15, y, 290, 36, bg, 5);

        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(COLOR_TEXT, bg);
        tft.drawString(items[i], 30, y + 18, 4);
      }

      tft.fillRect(0, 211, DISPLAY_WIDTH, 29, COLOR_HEADER);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_WHITE, COLOR_HEADER);
      tft.drawString("UP", 53, 225, 2);
      tft.drawString("DOWN", 160, 225, 2);
      tft.drawString("ENTER", 267, 225, 2);
    }


    // Update only the text of the old and new selected menu items.
    // No row clearing and no rectangle outlines are drawn here.
    void updateMainMenuSelection(uint8_t oldItem, uint8_t newItem) {
      if (!displayReady || oldItem == newItem) return;

      const char* items[] = {
        "Calibration", "Settings", "Sensor status", "About"
      };

      tft.setTextDatum(ML_DATUM);

      tft.setTextColor(COLOR_TEXT, COLOR_PANEL);
      tft.drawString(items[oldItem], 30, 55 + oldItem * 38 + 16, 4);

      tft.setTextColor(COLOR_TEXT, COLOR_HEADER);
      tft.drawString(items[newItem], 30, 55 + newItem * 38 + 16, 4);
    }

    void updateCalibrationMenuSelection(uint8_t oldItem, uint8_t newItem) {
      if (!displayReady || oldItem == newItem) return;

      const char* items[] = {
        "O2 - AIR 20.9 %",
        "O2 - PURE 100 %",
        "He - ZERO 0 %",
        "He - PURE 100 %"
      };

      tft.setTextDatum(ML_DATUM);

      tft.setTextColor(COLOR_TEXT, COLOR_PANEL);
      tft.drawString(items[oldItem], 30, 55 + oldItem * 38 + 16, 4);

      tft.setTextColor(COLOR_TEXT, COLOR_HEADER);
      tft.drawString(items[newItem], 30, 55 + newItem * 38 + 16, 4);
    }

    void updateSettingsMenuSelection(uint8_t oldItem, uint8_t newItem) {
      if (!displayReady || oldItem == newItem) return;

      const char* items[] = {
        "Display settings", "System settings"
      };

      tft.setTextDatum(ML_DATUM);

      tft.setTextColor(COLOR_TEXT, COLOR_PANEL);
      tft.drawString(items[oldItem], 30, 65 + oldItem * 45 + 18, 4);

      tft.setTextColor(COLOR_TEXT, COLOR_HEADER);
      tft.drawString(items[newItem], 30, 65 + newItem * 45 + 18, 4);
    }


    // O2 air calibration: capture progress screen.
    void drawOxygenCalibrationProgress(const char* calibrationName,
                                       uint8_t completed,
                                       uint8_t total,
                                       float latestVoltage_mV) {
      if (!displayReady) return;

      tft.fillScreen(COLOR_BG);
      tft.fillRect(0, 0, DISPLAY_WIDTH, 42, COLOR_HEADER);

      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_WHITE, COLOR_HEADER);
      tft.drawString(String("O2 ") + calibrationName + " CALIBRATION", 160, 21, 4);

      tft.setTextColor(COLOR_TEXT, COLOR_BG);
      tft.drawString("Taking measurements...", 160, 75, 2);

      tft.setTextColor(COLOR_O2, COLOR_BG);
      tft.drawString(String(completed) + " / " + String(total),
                     160, 112, 4);

      tft.setTextColor(COLOR_MUTED, COLOR_BG);
      if (isfinite(latestVoltage_mV)) {
        tft.drawString("Latest: " + String(latestVoltage_mV, 3) + " mV",
                       160, 150, 2);
      } else {
        tft.drawString("Waiting for first sample", 160, 150, 2);
      }

      // Simple progress bar.
      tft.drawRect(35, 177, 250, 12, COLOR_MUTED);
      uint16_t fillWidth = (total > 0)
        ? (uint16_t)(246UL * completed / total) : 0;
      if (fillWidth > 0) {
        tft.fillRect(37, 179, fillWidth, 8, COLOR_O2);
      }

      tft.fillRect(0, 211, DISPLAY_WIDTH, 29, COLOR_HEADER);
      tft.setTextColor(TFT_WHITE, COLOR_HEADER);
      tft.drawString("LONG ENTER - CANCEL", 160, 225, 2);
    }

    // O2 air calibration result screen.
    void drawOxygenCalibrationResult(const char* calibrationName,
                                     bool success,
                                     float average_mV,
                                     float range_mV,
                                     const char* detail) {
      if (!displayReady) return;

      tft.fillScreen(COLOR_BG);
      tft.fillRect(0, 0, DISPLAY_WIDTH, 42, COLOR_HEADER);

      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_WHITE, COLOR_HEADER);
      tft.drawString(String("O2 ") + calibrationName + " CALIBRATION", 160, 21, 4);

      tft.setTextColor(success ? COLOR_OK : COLOR_ERROR, COLOR_BG);
      tft.drawString(success ? "CALIBRATION SAVED" : "CALIBRATION FAILED",
                     160, 70, 2);

      tft.setTextColor(COLOR_TEXT, COLOR_BG);
      tft.drawString("Average: " + String(average_mV, 3) + " mV",
                     160, 108, 2);

      tft.setTextColor(COLOR_MUTED, COLOR_BG);
      tft.drawString("Range: " + String(range_mV, 3) + " mV",
                     160, 137, 2);

      tft.setTextColor(success ? COLOR_OK : COLOR_ERROR, COLOR_BG);
      tft.drawString(detail, 160, 169, 2);

      tft.fillRect(0, 211, DISPLAY_WIDTH, 29, COLOR_HEADER);
      tft.setTextColor(TFT_WHITE, COLOR_HEADER);
      if (success) {
        tft.drawString("ENTER - MENU", 160, 225, 2);
      } else {
        tft.drawString("ENTER - RETRY", 160, 225, 2);
      }
    }

    // Display a prerequisite message when a calibration cannot be started.
    void drawOxygenCalibrationBlocked(const char* calibrationName,
                                      const char* detail) {
      if (!displayReady) return;

      tft.fillScreen(COLOR_BG);
      tft.fillRect(0, 0, DISPLAY_WIDTH, 42, COLOR_HEADER);

      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_WHITE, COLOR_HEADER);
      tft.drawString(String("O2 ") + calibrationName, 160, 21, 4);

      tft.setTextColor(COLOR_ERROR, COLOR_BG);
      tft.drawString("CALIBRATION NOT AVAILABLE", 160, 85, 2);

      tft.setTextColor(COLOR_TEXT, COLOR_BG);
      tft.drawString(detail, 160, 125, 2);

      tft.fillRect(0, 211, DISPLAY_WIDTH, 29, COLOR_HEADER);
      tft.setTextColor(TFT_WHITE, COLOR_HEADER);
      tft.drawString("ENTER - MENU", 160, 225, 2);
    }

    // Helium calibration screens use the same layout and flow as O2.
    void drawHeliumCalibrationProgress(const char* calibrationName,
                                       uint8_t completed,
                                       uint8_t total,
                                       float latestVoltage_mV) {
      if (!displayReady) return;

      tft.fillScreen(COLOR_BG);
      tft.fillRect(0, 0, DISPLAY_WIDTH, 42, COLOR_HEADER);

      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_WHITE, COLOR_HEADER);
      tft.drawString(String("He ") + calibrationName + " CALIBRATION", 160, 21, 4);

      tft.setTextColor(COLOR_TEXT, COLOR_BG);
      tft.drawString("Taking measurements...", 160, 75, 2);

      tft.setTextColor(COLOR_HE, COLOR_BG);
      tft.drawString(String(completed) + " / " + String(total), 160, 112, 4);

      tft.setTextColor(COLOR_MUTED, COLOR_BG);
      if (isfinite(latestVoltage_mV)) {
        tft.drawString("Latest: " + String(latestVoltage_mV, 3) + " mV", 160, 150, 2);
      } else {
        tft.drawString("Waiting for first sample", 160, 150, 2);
      }

      tft.drawRect(35, 177, 250, 12, COLOR_MUTED);
      uint16_t fillWidth = (total > 0)
        ? (uint16_t)(246UL * completed / total) : 0;
      if (fillWidth > 0) tft.fillRect(37, 179, fillWidth, 8, COLOR_HE);

      tft.fillRect(0, 211, DISPLAY_WIDTH, 29, COLOR_HEADER);
      tft.setTextColor(TFT_WHITE, COLOR_HEADER);
      tft.drawString("LONG ENTER - CANCEL", 160, 225, 2);
    }

    void drawHeliumCalibrationResult(const char* calibrationName,
                                     bool success,
                                     float average_mV,
                                     float range_mV,
                                     const char* detail) {
      if (!displayReady) return;

      tft.fillScreen(COLOR_BG);
      tft.fillRect(0, 0, DISPLAY_WIDTH, 42, COLOR_HEADER);

      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_WHITE, COLOR_HEADER);
      tft.drawString(String("He ") + calibrationName + " CALIBRATION", 160, 21, 4);

      tft.setTextColor(success ? COLOR_OK : COLOR_ERROR, COLOR_BG);
      tft.drawString(success ? "CALIBRATION SAVED" : "CALIBRATION FAILED", 160, 70, 2);

      tft.setTextColor(COLOR_TEXT, COLOR_BG);
      tft.drawString("Average: " + String(average_mV, 3) + " mV", 160, 108, 2);

      tft.setTextColor(COLOR_MUTED, COLOR_BG);
      tft.drawString("Range: " + String(range_mV, 3) + " mV", 160, 137, 2);

      tft.setTextColor(success ? COLOR_OK : COLOR_ERROR, COLOR_BG);
      tft.drawString(detail, 160, 169, 2);

      tft.fillRect(0, 211, DISPLAY_WIDTH, 29, COLOR_HEADER);
      tft.setTextColor(TFT_WHITE, COLOR_HEADER);
      tft.drawString(success ? "ENTER - MENU" : "ENTER - RETRY", 160, 225, 2);
    }

    void drawHeliumCalibrationBlocked(const char* calibrationName,
                                      const char* detail) {
      if (!displayReady) return;

      tft.fillScreen(COLOR_BG);
      tft.fillRect(0, 0, DISPLAY_WIDTH, 42, COLOR_HEADER);

      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_WHITE, COLOR_HEADER);
      tft.drawString(String("He ") + calibrationName, 160, 21, 4);

      tft.setTextColor(COLOR_ERROR, COLOR_BG);
      tft.drawString("CALIBRATION NOT AVAILABLE", 160, 85, 2);

      tft.setTextColor(COLOR_TEXT, COLOR_BG);
      tft.drawString(detail, 160, 125, 2);

      tft.fillRect(0, 211, DISPLAY_WIDTH, 29, COLOR_HEADER);
      tft.setTextColor(TFT_WHITE, COLOR_HEADER);
      tft.drawString("ENTER - MENU", 160, 225, 2);
    }

    void drawSensorStatus(bool adcReady,
                          bool oxygenReady,
                          bool heliumReady,
                          float oxygenVoltage_mV,
                          bool oxygenVoltageValid,
                          float heliumVoltage_mV,
                          bool heliumVoltageValid) {
      if (!displayReady) return;

      tft.fillScreen(COLOR_BG);
      tft.fillRect(0, 0, DISPLAY_WIDTH, 42, COLOR_HEADER);

      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_WHITE, COLOR_HEADER);
      tft.drawString("SENSOR STATUS", 160, 21, 4);

      // ADS1115 status
      tft.setTextDatum(ML_DATUM);
      tft.setTextColor(COLOR_TEXT, COLOR_BG);
      tft.drawString("ADS1115", 20, 65, 2);
      tft.setTextDatum(MR_DATUM);
      tft.setTextColor(adcReady ? COLOR_OK : COLOR_ERROR, COLOR_BG);
      tft.drawString(adcReady ? "READY" : "ERROR", 300, 65, 2);

      // Oxygen sensor status and measured differential voltage
      tft.setTextDatum(ML_DATUM);
      tft.setTextColor(COLOR_TEXT, COLOR_BG);
      tft.drawString("Oxygen sensor", 20, 105, 2);
      tft.setTextDatum(MR_DATUM);
      tft.setTextColor(oxygenReady ? COLOR_OK : COLOR_ERROR, COLOR_BG);
      tft.drawString(oxygenReady ? "READY" : "ERROR", 300, 105, 2);
      updateSensorVoltageLine("O2", oxygenVoltage_mV, oxygenVoltageValid, 126);

      // Helium sensor status and measured differential voltage
      tft.setTextDatum(ML_DATUM);
      tft.setTextColor(COLOR_TEXT, COLOR_BG);
      tft.drawString("Helium sensor", 20, 160, 2);
      tft.setTextDatum(MR_DATUM);
      tft.setTextColor(heliumReady ? COLOR_OK : COLOR_ERROR, COLOR_BG);
      tft.drawString(heliumReady ? "READY" : "ERROR", 300, 160, 2);
      updateSensorVoltageLine("He", heliumVoltage_mV, heliumVoltageValid, 181);

      tft.fillRect(0, 211, DISPLAY_WIDTH, 29, COLOR_HEADER);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_WHITE, COLOR_HEADER);
      tft.drawString("LONG ENTER - BACK", 160, 225, 2);
    }

    void updateSensorVoltageLine(const char* sensorName,
                                 float voltage_mV,
                                 bool voltageValid,
                                 int y) {
      if (!displayReady) return;

      tft.fillRect(20, y - 9, 280, 20, COLOR_BG);
      tft.setTextDatum(ML_DATUM);
      tft.setTextColor(COLOR_MUTED, COLOR_BG);
      tft.drawString(String(sensorName) + " voltage:", 28, y, 2);

      tft.setTextDatum(MR_DATUM);
      tft.setTextColor(voltageValid ? COLOR_TEXT : COLOR_ERROR, COLOR_BG);
      tft.drawString(voltageValid ? String(voltage_mV, 3) + " mV" : "-- mV", 292, y, 2);
    }

    void drawAbout() {
      if (!displayReady) return;

      tft.fillScreen(COLOR_BG);
      tft.fillRect(0, 0, DISPLAY_WIDTH, 42, COLOR_HEADER);

      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_WHITE, COLOR_HEADER);
      tft.drawString("ABOUT", 160, 21, 4);

      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(COLOR_TEXT, COLOR_BG);
      tft.drawString(PROJECT_NAME, 160, 90, 4);

      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(COLOR_MUTED, COLOR_BG);
      tft.drawString(String("Version ") + PROJECT_VERSION, 160, 130, 2);

      tft.fillRect(0, 211, DISPLAY_WIDTH, 29, COLOR_HEADER);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_WHITE, COLOR_HEADER);
      tft.drawString("LONG ENTER - BACK", 160, 225, 2);
    }

    void updateOxygen(float value) {

      if (!displayReady) return;

      tft.fillRect(20, 95, 124, 50, COLOR_PANEL);

      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(COLOR_TEXT, COLOR_PANEL);

      if (isnan(value)) {
        tft.drawString("--.- %", 82, 120, 4);
      } else {
        tft.drawString(String(value, 1) + " %", 82, 120, 4);
      }
    }

    void updateHelium(float value) {

      if (!displayReady) return;

      tft.fillRect(176, 95, 124, 50, COLOR_PANEL);

      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(COLOR_TEXT, COLOR_PANEL);

      if (isnan(value)) {
        tft.drawString("--.- %", 238, 120, 4);
      } else {
        tft.drawString(String(value, 1) + " %", 238, 120, 4);
      }
    }

};

#endif
