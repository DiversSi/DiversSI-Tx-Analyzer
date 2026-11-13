
#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <math.h>
#include "Config.h"

class DisplayManager {

  private:

    TFT_eSPI tft = TFT_eSPI();

    // Display colors
    uint16_t COLOR_BG       = TFT_BLACK;
    uint16_t COLOR_ACCENT   = TFT_BLUE;
    uint16_t COLOR_HEADER   = TFT_BLUE;
    uint16_t COLOR_TEXT     = TFT_WHITE;
    uint16_t COLOR_HEADER_TEXT = TFT_WHITE;
    uint16_t COLOR_MUTED    = TFT_WHITE;
    uint16_t COLOR_OK       = TFT_GREEN;
    uint16_t COLOR_ERROR    = TFT_RED;
    uint16_t COLOR_PANEL    = TFT_BLACK;
    uint8_t brightnessPercent = 100;
    bool lightTheme = false;

    bool displayReady = false;

    void drawHeader() {

      tft.fillRect(0, 0, DISPLAY_WIDTH, 42, COLOR_HEADER);

      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER);

      tft.drawString(PROJECT_NAME, 145, 21, 4);
    }

    void drawSensorPanels() {

      // O2 panel
      tft.fillRoundRect(8, 52, 148, 125, COLOR_BG, 8);
      // 4-pixel O2 border (TFT_eSPI drawRoundRect has no thickness argument)
      tft.drawRoundRect(8, 52, 148, 125, 8, COLOR_ACCENT);
      tft.drawRoundRect(9, 53, 146, 123, 7, COLOR_ACCENT);
      tft.drawRoundRect(10, 54, 144, 121, 6, COLOR_ACCENT);
      tft.drawRoundRect(11, 55, 142, 119, 5, COLOR_ACCENT);

      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(COLOR_ACCENT, COLOR_BG);
      tft.drawString("OXYGEN", 82, 72, 4);

      tft.setTextColor(COLOR_TEXT, COLOR_BG);
      tft.drawString("--.- %", 82, 120, 4);

      // Helium panel
      tft.fillRoundRect(164, 52, 148, 125, COLOR_BG, 8);
      // 4-pixel helium border
      tft.drawRoundRect(164, 52, 148, 125, 8, COLOR_ACCENT);
      tft.drawRoundRect(165, 53, 146, 123, 7, COLOR_ACCENT);
      tft.drawRoundRect(166, 54, 144, 121, 6, COLOR_ACCENT);
      tft.drawRoundRect(167, 55, 142, 119, 5, COLOR_ACCENT);

      tft.setTextColor(COLOR_ACCENT, COLOR_BG);
      tft.drawString("HELIUM", 238, 72, 4);

      tft.setTextColor(COLOR_TEXT, COLOR_BG);
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
      tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER);

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

    static const uint16_t* accentPalette() {
      static const uint16_t colors[] = {TFT_BLUE, 0x03AF, 0x03E0, 0xA320, 0x4010, 0x0328};
      return colors;
    }

    static const char** accentNames() {
      static const char* names[] = {"Blue", "Cyan", "Green", "Orange", "Purple", "Teal"};
      return names;
    }

    static const uint16_t* backgroundPalette(bool light) {
      static const uint16_t darkColors[] = {TFT_BLACK, TFT_NAVY, 0x4208, 0x0200, 0x3006};
      static const uint16_t lightColors[] = {TFT_WHITE, 0xD69A, 0xFFDE, 0xDFFF, 0xDFFB};
      return light ? lightColors : darkColors;
    }

    static const char** backgroundNames(bool light) {
      static const char* darkNames[] = {"Black", "Navy", "Charcoal", "Green", "Purple"};
      static const char* lightNames[] = {"White", "Grey", "Cream", "Pale blue", "Pale green"};
      return light ? lightNames : darkNames;
    }

    static const uint16_t* textPalette(bool light) {
      static const uint16_t darkColors[] = {TFT_WHITE, TFT_LIGHTGREY, TFT_CYAN};
      static const uint16_t lightColors[] = {TFT_BLACK, TFT_DARKGREY, TFT_NAVY};
      return light ? lightColors : darkColors;
    }

    static const char** textNames(bool light) {
      static const char* darkNames[] = {"White", "Light grey", "Cyan"};
      static const char* lightNames[] = {"Black", "Dark grey", "Navy"};
      return light ? lightNames : darkNames;
    }

    void setTheme(bool light, uint8_t accentIndex = 0, uint8_t backgroundIndex = 0,
                  uint8_t textIndex = 0) {
      lightTheme = light;
      if (accentIndex > 5) accentIndex = 0;
      if (backgroundIndex > 4) backgroundIndex = 0;
      if (textIndex > 2) textIndex = 0;
      COLOR_ACCENT = accentPalette()[accentIndex];
      COLOR_BG = backgroundPalette(light)[backgroundIndex];
      COLOR_HEADER = COLOR_ACCENT;
      COLOR_PANEL = COLOR_BG;
      COLOR_TEXT = textPalette(light)[textIndex];
      // Use only the selected three colors: Dark uses text on the accent;
      // Light uses the background color on the accent.
      COLOR_HEADER_TEXT = light ? COLOR_BG : COLOR_TEXT;
      COLOR_MUTED = COLOR_TEXT;
      COLOR_OK = TFT_GREEN;
      COLOR_ERROR = TFT_RED;
    }

    // Small IP5306 battery indicator in the upper-right corner.
    // percent is the PMIC's coarse 0/25/50/75/100% gauge; -1 means unavailable.
    void updateBatteryIndicator(int percent, bool charging, bool full) {
      if (!displayReady) return;

      const int x = 284, y = 11, w = 25, h = 13;
      uint16_t outline = COLOR_HEADER_TEXT;
      uint16_t fill = COLOR_OK;
      tft.fillRect(270, 5, 49, 31, COLOR_HEADER);
      tft.drawRoundRect(x, y, w, h, 2, outline);
      tft.fillRect(x + w, y + 4, 3, 5, outline);

      if (percent >= 0) {
        if (percent <= 25) fill = TFT_RED;
        else if (percent <= 50) fill = TFT_YELLOW;
        else fill = COLOR_OK;
        int innerWidth = (w - 4) * percent / 100;
        if (innerWidth > 0) tft.fillRect(x + 2, y + 2, innerWidth, h - 4, fill);
      } else {
        tft.drawLine(x + 5, y + 6, x + 18, y + 6, outline);
      }

      if (charging && !full) {
        // Simple charging bolt, avoiding font-dependent Unicode symbols.
        tft.drawLine(x + 13, y + 1, x + 9, y + 7, TFT_YELLOW);
        tft.drawLine(x + 9, y + 7, x + 14, y + 7, TFT_YELLOW);
        tft.drawLine(x + 14, y + 7, x + 11, y + 12, TFT_YELLOW);
      }
    }

    void drawDisplaySettingsRow(uint8_t row, uint8_t selected, uint8_t brightness,
                                uint8_t theme, uint8_t dimTimeout,
                                uint8_t accentIndex, uint8_t backgroundIndex, uint8_t textIndex) {
      if (row >= 6) return;
      const char* timeoutLabels[] = {"30 seconds", "1 minute", "2 minutes", "5 minutes"};
      bool light = theme == 1;
      const char* labels[] = {"Brightness", "Color scheme", "Dim timeout", "Accent color", "Background", "Text color"};
      String values[6] = {
        String(brightness) + " %", light ? "Light" : "Dark", timeoutLabels[dimTimeout],
        accentNames()[accentIndex], backgroundNames(light)[backgroundIndex], textNames(light)[textIndex]
      };
      uint16_t swatch = 0;
      if (row == 3) swatch = accentPalette()[accentIndex];
      else if (row == 4) swatch = backgroundPalette(light)[backgroundIndex];
      else if (row == 5) swatch = textPalette(light)[textIndex];

      const int y = 46 + row * 27;
      const bool selectedRow = (row == selected);
      const int labelWidth = 174;
      // No rounded row backgrounds: only the selected label gets a plain rectangular highlight.
      tft.setTextDatum(ML_DATUM);
      tft.setTextColor(selectedRow ? COLOR_HEADER_TEXT : COLOR_TEXT,
                       selectedRow ? COLOR_ACCENT : COLOR_BG);
      tft.drawString(labels[row], 20, y + 12, 2);

      tft.fillRect(190, y + 2, 116, 20, COLOR_BG);
      tft.setTextDatum(MR_DATUM);
      tft.setTextColor(COLOR_TEXT, COLOR_BG);
      tft.drawString(values[row], (row >= 3) ? 270 : 298, y + 12, 2);
      if (row >= 3) {
        tft.drawRect(278, y + 5, 23, 14, COLOR_TEXT);
        tft.fillRect(280, y + 7, 19, 10, swatch);
      }
    }

    void drawDisplaySettings(uint8_t selected, uint8_t brightness,
                             uint8_t theme, uint8_t dimTimeout,
                             uint8_t accentIndex, uint8_t backgroundIndex, uint8_t textIndex) {
      if (!displayReady) return;
      tft.fillRect(0, 0, DISPLAY_WIDTH, 42, COLOR_HEADER);
      tft.setTextDatum(MC_DATUM); tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER);
      tft.drawString("DISPLAY SETTINGS", 160, 21, 4);
      tft.fillRect(0, 42, DISPLAY_WIDTH, 169, COLOR_BG);
      for (uint8_t i = 0; i < 6; ++i)
        drawDisplaySettingsRow(i, selected, brightness, theme, dimTimeout,
                               accentIndex, backgroundIndex, textIndex);
      tft.fillRect(0, 211, DISPLAY_WIDTH, 29, COLOR_HEADER);
      tft.setTextDatum(MC_DATUM); tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER);
      tft.drawString("UP/DOWN - SELECT  ENTER - CHANGE", 160, 225, 2);
    }

    void updateDisplaySettingsSelection(uint8_t oldItem, uint8_t newItem,
                                        uint8_t brightness, uint8_t theme,
                                        uint8_t dimTimeout, uint8_t accentIndex,
                                        uint8_t backgroundIndex, uint8_t textIndex) {
      if (!displayReady || oldItem == newItem) return;
      drawDisplaySettingsRow(oldItem, newItem, brightness, theme, dimTimeout,
                             accentIndex, backgroundIndex, textIndex);
      drawDisplaySettingsRow(newItem, newItem, brightness, theme, dimTimeout,
                             accentIndex, backgroundIndex, textIndex);
    }

    void updateDisplaySettingsValue(uint8_t selected, uint8_t brightness,
                                    uint8_t theme, uint8_t dimTimeout,
                                    uint8_t accentIndex, uint8_t backgroundIndex,
                                    uint8_t textIndex) {
      if (!displayReady) return;
      drawDisplaySettingsRow(selected, selected, brightness, theme, dimTimeout,
                             accentIndex, backgroundIndex, textIndex);
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
      tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER);
      tft.drawString("CALIBRATION MENU", 160, 21, 4);

      // Menu items
      for (uint8_t i = 0; i < 4; i++) {

        int y = 55 + (i * 38);

        uint16_t bgColor = (i == selectedItem)
                           ? COLOR_ACCENT
                           : COLOR_PANEL;


        tft.setTextDatum(ML_DATUM);
        tft.setTextColor((i == selectedItem) ? COLOR_HEADER_TEXT : COLOR_TEXT, bgColor);
        tft.drawString(menuItems[i], 30, y + 16, 4);
      }
      
      // Footer
      tft.fillRect(0, 211, DISPLAY_WIDTH, 29, COLOR_HEADER);

      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER);

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

      // Redraw only menu regions; avoids a full-screen flash when returning here.
      tft.fillRect(0, 0, DISPLAY_WIDTH, 42, COLOR_HEADER);
      tft.fillRect(0, 42, DISPLAY_WIDTH, 169, COLOR_BG);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER);
      tft.drawString("MAIN MENU", 160, 21, 4);

      for (uint8_t i = 0; i < 4; i++) {
        int y = 55 + i * 38;

        uint16_t bg = (i == selectedItem) ? COLOR_HEADER : COLOR_PANEL;


        tft.setTextDatum(ML_DATUM);
        tft.setTextColor((i == selectedItem) ? COLOR_HEADER_TEXT : COLOR_TEXT, bg);
        tft.drawString(items[i], 30, y + 16, 4);
      }

      tft.fillRect(0, 211, DISPLAY_WIDTH, 29, COLOR_HEADER);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER);
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

      tft.fillRect(0, 0, DISPLAY_WIDTH, 42, COLOR_HEADER);
      tft.fillRect(0, 42, DISPLAY_WIDTH, 169, COLOR_BG);

      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER);
      tft.drawString("SETTINGS", 160, 21, 4);

      for (uint8_t i = 0; i < 2; i++) {
        int y = 65 + i * 45;

        uint16_t bg = (i == selectedItem) ? COLOR_HEADER : COLOR_PANEL;


        tft.setTextDatum(ML_DATUM);
        tft.setTextColor((i == selectedItem) ? COLOR_HEADER_TEXT : COLOR_TEXT, bg);
        tft.drawString(items[i], 30, y + 18, 4);
      }

      tft.fillRect(0, 211, DISPLAY_WIDTH, 29, COLOR_HEADER);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER);
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

      tft.setTextColor(COLOR_TEXT, COLOR_BG);
      tft.drawString(items[oldItem], 30, 55 + oldItem * 38 + 16, 4);

      tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER);
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

      tft.setTextColor(COLOR_TEXT, COLOR_BG);
      tft.drawString(items[oldItem], 30, 55 + oldItem * 38 + 16, 4);

      tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER);
      tft.drawString(items[newItem], 30, 55 + newItem * 38 + 16, 4);
    }

    void updateSettingsMenuSelection(uint8_t oldItem, uint8_t newItem) {
      if (!displayReady || oldItem == newItem) return;

      const char* items[] = {
        "Display settings", "System settings"
      };

      tft.setTextDatum(ML_DATUM);

      tft.setTextColor(COLOR_TEXT, COLOR_BG);
      tft.drawString(items[oldItem], 30, 65 + oldItem * 45 + 18, 4);

      tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER);
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
      tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER);
      tft.drawString(String("O2 ") + calibrationName + " CALIBRATION", 160, 21, 4);

      tft.setTextColor(COLOR_TEXT, COLOR_BG);
      tft.drawString("Taking measurements...", 160, 75, 2);

      tft.setTextColor(COLOR_ACCENT, COLOR_BG);
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
        tft.fillRect(37, 179, fillWidth, 8, COLOR_ACCENT);
      }

      tft.fillRect(0, 211, DISPLAY_WIDTH, 29, COLOR_HEADER);
      tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER);
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
      tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER);
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
      tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER);
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
      tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER);
      tft.drawString(String("O2 ") + calibrationName, 160, 21, 4);

      tft.setTextColor(COLOR_ERROR, COLOR_BG);
      tft.drawString("CALIBRATION NOT AVAILABLE", 160, 85, 2);

      tft.setTextColor(COLOR_TEXT, COLOR_BG);
      tft.drawString(detail, 160, 125, 2);

      tft.fillRect(0, 211, DISPLAY_WIDTH, 29, COLOR_HEADER);
      tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER);
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
      tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER);
      tft.drawString(String("He ") + calibrationName + " CALIBRATION", 160, 21, 4);

      tft.setTextColor(COLOR_TEXT, COLOR_BG);
      tft.drawString("Taking measurements...", 160, 75, 2);

      tft.setTextColor(COLOR_ACCENT, COLOR_BG);
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
      if (fillWidth > 0) tft.fillRect(37, 179, fillWidth, 8, COLOR_ACCENT);

      tft.fillRect(0, 211, DISPLAY_WIDTH, 29, COLOR_HEADER);
      tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER);
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
      tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER);
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
      tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER);
      tft.drawString(success ? "ENTER - MENU" : "ENTER - RETRY", 160, 225, 2);
    }

    void drawHeliumCalibrationBlocked(const char* calibrationName,
                                      const char* detail) {
      if (!displayReady) return;

      tft.fillScreen(COLOR_BG);
      tft.fillRect(0, 0, DISPLAY_WIDTH, 42, COLOR_HEADER);

      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER);
      tft.drawString(String("He ") + calibrationName, 160, 21, 4);

      tft.setTextColor(COLOR_ERROR, COLOR_BG);
      tft.drawString("CALIBRATION NOT AVAILABLE", 160, 85, 2);

      tft.setTextColor(COLOR_TEXT, COLOR_BG);
      tft.drawString(detail, 160, 125, 2);

      tft.fillRect(0, 211, DISPLAY_WIDTH, 29, COLOR_HEADER);
      tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER);
      tft.drawString("ENTER - MENU", 160, 225, 2);
    }


    void drawWifiStatusIcon(int cx, int cy, uint16_t color) {
      // Three simple radio-wave arcs and a dot, drawn without font glyph dependencies.
      const float radii[] = {7.0f, 13.0f, 19.0f};
      for (uint8_t r = 0; r < 3; ++r) {
        int px = 0, py = 0;
        bool first = true;
        for (int a = 225; a <= 315; a += 10) {
          float rad = a * 0.0174532925f;
          int x = cx + (int)lroundf(cosf(rad) * radii[r]);
          int y = cy + (int)lroundf(sinf(rad) * radii[r]);
          if (!first) tft.drawLine(px, py, x, y, color);
          px = x; py = y; first = false;
        }
      }
      tft.fillCircle(cx, cy + 1, 2, color);
    }

    void drawSystemSettingsRow(uint8_t row, uint8_t selectedItem, bool wifiEnabled,
                               bool wifiConnected, bool wifiConnecting) {
      if (row >= 2) return;
      int y = 65 + row * 45;
      uint16_t bg = (row == selectedItem) ? COLOR_ACCENT : COLOR_BG;
      uint16_t fg = (row == selectedItem) ? COLOR_HEADER_TEXT : COLOR_TEXT;
      tft.setTextDatum(ML_DATUM); tft.setTextColor(fg, bg);
      if (row == 0) {
        tft.drawString("Wi-Fi", 24, y + 19, 4);
        // Keep the ON/OFF value and Wi-Fi icon on a constant background.
        tft.fillRect(210, y, 101, 38, COLOR_BG);
        tft.setTextDatum(MR_DATUM); tft.setTextColor(COLOR_TEXT, COLOR_BG);
        tft.drawString(wifiEnabled ? "ON" : "OFF", 270, y + 19, 4);
        uint16_t iconColor = wifiConnected ? TFT_GREEN :
                             (wifiConnecting ? TFT_YELLOW : COLOR_TEXT);
        drawWifiStatusIcon(291, y + 19, iconColor);
      } else {
        tft.drawString("Firmware update", 24, y + 19, 4);
      }
    }

    void drawSystemSettingsMenu(uint8_t selectedItem, bool wifiEnabled, bool wifiConnected,
                                bool wifiConnecting = false) {
      if (!displayReady) return;
      tft.fillRect(0, 0, DISPLAY_WIDTH, 42, COLOR_HEADER);
      tft.fillRect(0, 42, DISPLAY_WIDTH, 169, COLOR_BG);
      tft.setTextDatum(MC_DATUM); tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER);
      tft.drawString("SYSTEM SETTINGS", 160, 21, 4);
      for (uint8_t i = 0; i < 2; ++i)
        drawSystemSettingsRow(i, selectedItem, wifiEnabled, wifiConnected, wifiConnecting);
      tft.fillRect(0, 211, DISPLAY_WIDTH, 29, COLOR_HEADER);
      tft.setTextDatum(MC_DATUM); tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER);
      tft.drawString("UP/DOWN - SELECT  ENTER - OPEN/TOGGLE", 160, 225, 2);
    }

    void updateSystemSettingsWifiStatus(uint8_t selectedItem, bool wifiEnabled,
                                        bool wifiConnected, bool wifiConnecting) {
      if (!displayReady) return;
      drawSystemSettingsRow(0, selectedItem, wifiEnabled, wifiConnected, wifiConnecting);
    }

    // Draw one Wi-Fi network row. The SSID follows the selected text colors,
    // while signal strength always keeps the normal text colors.
    void drawWifiNetworkRow(uint8_t index, uint8_t row, uint8_t selectedItem,
                            const String* names, const int32_t* signals) {
      int y = 53 + row * 38;
      uint16_t bg = (index == selectedItem) ? COLOR_ACCENT : COLOR_PANEL;
      uint16_t fg = (index == selectedItem) ? COLOR_HEADER_TEXT : COLOR_TEXT;

      String label = names[index];
      if (label.length() == 0) label = "Hidden network";
      if (label.length() > 14) label = label.substring(0, 13) + "~";

      // Clear the SSID and signal text areas before repainting them.
      // This removes trailing characters when the next network name is shorter.
      tft.fillRect(10, y, 230, 32, bg);
      tft.fillRect(245, y, 65, 32, COLOR_PANEL);

      tft.setTextDatum(ML_DATUM);
      tft.setTextColor(fg, bg);
      tft.drawString(label, 18, y + 16, 4);

      tft.setTextDatum(MR_DATUM);
      tft.setTextColor(COLOR_TEXT, COLOR_PANEL);
      tft.drawString(String(signals[index]) + " dBm", 300, y + 16, 2);
    }

    // Update only the affected Wi-Fi rows when the selection moves.
    // If the four-row viewport scrolls, redraw its visible rows, but not the screen.
    void updateWifiNetworksSelection(uint8_t oldItem, uint8_t newItem,
                                     uint8_t count,
                                     const String* names, const int32_t* signals) {
      if (!displayReady || count == 0 || names == nullptr || signals == nullptr) return;

      uint8_t visible = count > 4 ? 4 : count;
      uint8_t oldFirst = oldItem >= visible ? oldItem - visible + 1 : 0;
      uint8_t newFirst = newItem >= visible ? newItem - visible + 1 : 0;

      if (oldFirst != newFirst) {
        for (uint8_t row = 0; row < visible; ++row) {
          uint8_t index = newFirst + row;
          if (index >= count) break;
          drawWifiNetworkRow(index, row, newItem, names, signals);
        }
        return;
      }

      if (oldItem >= oldFirst && oldItem < oldFirst + visible) {
        drawWifiNetworkRow(oldItem, oldItem - oldFirst, newItem, names, signals);
      }
      if (newItem >= newFirst && newItem < newFirst + visible && newItem != oldItem) {
        drawWifiNetworkRow(newItem, newItem - newFirst, newItem, names, signals);
      }
    }

    void drawWifiNetworks(uint8_t selectedItem, uint8_t count,
                          bool scanning, bool failed,
                          const String* names, const int32_t* signals) {
      if (!displayReady) return;
      tft.fillScreen(COLOR_BG);
      tft.fillRect(0, 0, DISPLAY_WIDTH, 42, COLOR_HEADER);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER);
      tft.drawString("WI-FI NETWORKS", 160, 21, 4);

      if (scanning) {
        tft.setTextColor(COLOR_TEXT, COLOR_BG);
        tft.drawString("Scanning for networks...", 160, 112, 2);
      } else if (failed) {
        tft.setTextColor(COLOR_ERROR, COLOR_BG);
        tft.drawString("Wi-Fi scan failed", 160, 105, 2);
        tft.setTextColor(COLOR_MUTED, COLOR_BG);
        tft.drawString("Press ENTER to retry", 160, 135, 2);
      } else if (count == 0) {
        tft.setTextColor(COLOR_TEXT, COLOR_BG);
        tft.drawString("No networks found", 160, 112, 2);
      } else {
        uint8_t visible = count > 4 ? 4 : count;
        uint8_t first = selectedItem >= visible ? selectedItem - visible + 1 : 0;
        for (uint8_t row = 0; row < visible; ++row) {
          uint8_t i = first + row;
          if (i >= count) break;
          drawWifiNetworkRow(i, row, selectedItem, names, signals);
        }
      }
      tft.fillRect(0, 211, DISPLAY_WIDTH, 29, COLOR_HEADER);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER);
      tft.drawString("UP/DOWN - SELECT  ENTER - CHOOSE", 160, 225, 2);
    }

    void drawWifiPassword(const String& ssid, const String& maskedPassword,
                          uint8_t selectedKey, bool symbolsPage, bool uppercase,
                          bool connecting, bool connected, bool failed) {
      if (!displayReady) return;
      tft.fillScreen(COLOR_BG);
      tft.fillRect(0, 0, DISPLAY_WIDTH, 42, COLOR_HEADER);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER);
      tft.drawString("WI-FI PASSWORD", 160, 21, 4);

      tft.setTextDatum(ML_DATUM);
      tft.setTextColor(COLOR_MUTED, COLOR_BG);
      tft.drawString("Network:", 10, 58, 2);
      tft.setTextColor(COLOR_TEXT, COLOR_BG);
      String shownSSID = ssid;
      if (shownSSID.length() > 27) shownSSID = shownSSID.substring(0, 26) + "~";
      tft.drawString(shownSSID, 82, 58, 2);

      tft.setTextColor(COLOR_MUTED, COLOR_BG);
      tft.drawString("Password:", 10, 84, 2);
      tft.setTextColor(COLOR_TEXT, COLOR_BG);
      String shownPassword = maskedPassword;
      if (shownPassword.length() > 28) shownPassword = shownPassword.substring(shownPassword.length() - 28);
      tft.drawString(shownPassword, 90, 84, 2);

      if (connecting || connected || failed) {
        tft.fillRoundRect(18, 119, 284, 76, COLOR_PANEL, 6);
        tft.setTextDatum(MC_DATUM);
        if (connecting) {
          tft.setTextColor(COLOR_MUTED, COLOR_BG);
          tft.drawString("Connecting to Wi-Fi...", 160, 157, 4);
        } else if (connected) {
          tft.setTextColor(COLOR_OK, COLOR_BG);
          tft.drawString("Wi-Fi connected", 160, 157, 4);
        } else {
          tft.setTextColor(COLOR_ERROR, COLOR_BG);
          tft.drawString("Connection failed", 160, 157, 4);
        }
      } else {
        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(COLOR_MUTED, COLOR_BG);
        tft.drawString(symbolsPage ? "Symbols" : (uppercase ? "ABC" : "abc"), 10, 105, 2);

        const uint8_t cols = 9;
        const int cellW = 34;
        const int cellH = 17;
        const int x0 = 7;
        const int y0 = 115;
        for (uint8_t i = 0; i < 37; ++i) {
          uint8_t row = i / cols;
          uint8_t col = i % cols;
          int x = x0 + col * cellW;
          int y = y0 + row * cellH;
          bool selected = (i == selectedKey);
          uint16_t bg = selected ? COLOR_HEADER : COLOR_PANEL;
          uint16_t fg = selected ? COLOR_HEADER_TEXT : COLOR_TEXT;
          tft.fillRoundRect(x, y, 32, 15, 3, bg);
          tft.setTextDatum(MC_DATUM);
          tft.setTextColor(fg, bg);
          String key;
          if ((symbolsPage && i < 32) || (!symbolsPage && i < 36)) {
            if (symbolsPage) {
              const char symbols[] = "!@#$%^&*()-_=+[]{};:,.?/\\|<>~`\"'";
              key = String(symbols[i]);
            } else if (i < 26) {
              key = String(char((uppercase ? 'A' : 'a') + i));
            } else {
              key = String(char('0' + i - 26));
            }
          } else {
            const char* labels[] = {"SHFT", "?123", "SPACE", "DEL", "DONE"};
            key = labels[i - 32];
          }
          tft.drawString(key, x + 16, y + 8, 2);
        }
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COLOR_MUTED, COLOR_BG);
        tft.drawString("UP/DOWN: MOVE   ENTER: SELECT", 160, 205, 2);
      }

      tft.fillRect(0, 211, DISPLAY_WIDTH, 29, COLOR_HEADER);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER);
      tft.drawString("LONG ENTER - CANCEL", 160, 225, 2);
    }

    void drawFirmwareUpdateInfo(bool checked = false, bool success = false,
                                const String& latestVersion = String(),
                                const String& status = String("Press ENTER to check for updates")) {
      if (!displayReady) return;
      tft.fillScreen(COLOR_BG);
      tft.fillRect(0, 0, DISPLAY_WIDTH, 42, COLOR_HEADER);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER);
      tft.drawString("FIRMWARE UPDATE", 160, 21, 4);

      // Use Font 4 for the version values so they are easy to read.
      tft.setTextDatum(ML_DATUM);
      tft.setTextColor(COLOR_TEXT, COLOR_BG);
      tft.drawString(String("Installed: ") + PROJECT_VERSION, 24, 78, 4);
      tft.drawString(String("Available: ") + (success ? latestVersion : "--"), 24, 112, 4);

      // Render status in Font 4, splitting longer messages across two lines.
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(checked ? (success ? COLOR_OK : COLOR_ERROR) : COLOR_MUTED, COLOR_BG);
      String statusLine1 = status;
      String statusLine2 = "";
      if (status.length() > 19) {
        int splitAt = status.lastIndexOf(' ', 19);
        if (splitAt > 0) {
          statusLine1 = status.substring(0, splitAt);
          statusLine2 = status.substring(splitAt + 1);
        }
      }
      tft.drawString(statusLine1, 160, statusLine2.length() ? 158 : 170, 4);
      if (statusLine2.length()) {
        tft.drawString(statusLine2, 160, 185, 4);
      }

      tft.fillRect(0, 211, DISPLAY_WIDTH, 29, COLOR_HEADER);
      tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER);
      tft.drawString("ENTER: CHECK   LONG ENTER: BACK", 160, 225, 2);
    }

    void updateSystemSettingsSelection(uint8_t oldItem, uint8_t newItem,
                                       bool wifiEnabled, bool wifiConnected,
                                       bool wifiConnecting = false) {
      if (!displayReady || oldItem == newItem) return;
      drawSystemSettingsRow(oldItem, newItem, wifiEnabled, wifiConnected, wifiConnecting);
      drawSystemSettingsRow(newItem, newItem, wifiEnabled, wifiConnected, wifiConnecting);
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
      tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER);
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
      tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER);
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
      tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER);
      tft.drawString("ABOUT", 160, 21, 4);

      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(COLOR_TEXT, COLOR_BG);
      tft.drawString(PROJECT_NAME, 160, 90, 4);

      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(COLOR_MUTED, COLOR_BG);
      tft.drawString(String("Version ") + PROJECT_VERSION, 160, 130, 2);

      tft.fillRect(0, 211, DISPLAY_WIDTH, 29, COLOR_HEADER);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER);
      tft.drawString("LONG ENTER - BACK", 160, 225, 2);
    }

    void updateOxygen(float value) {

      if (!displayReady) return;

      tft.fillRect(20, 95, 124, 50, COLOR_BG);

      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(COLOR_TEXT, COLOR_BG);

      if (isnan(value)) {
        tft.drawString("--.- %", 82, 120, 4);
      } else {
        tft.drawString(String(value, 1) + " %", 82, 120, 4);
      }
    }

    void updateHelium(float value) {

      if (!displayReady) return;

      tft.fillRect(176, 95, 124, 50, COLOR_BG);

      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(COLOR_TEXT, COLOR_BG);

      if (isnan(value)) {
        tft.drawString("--.- %", 238, 120, 4);
      } else {
        tft.drawString(String(value, 1) + " %", 238, 120, 4);
      }
    }

};

#endif
