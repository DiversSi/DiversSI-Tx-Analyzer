#ifndef MENU_MANAGER_H
#define MENU_MANAGER_H

#include <Arduino.h>
#include "ButtonManager.h"

enum AppScreen {
  SCREEN_MAIN,
  SCREEN_MAIN_MENU,
  SCREEN_CALIBRATION_MENU,
  SCREEN_SETTINGS_MENU,
  SCREEN_DISPLAY_SETTINGS,
  SCREEN_SENSOR_STATUS,
  SCREEN_ABOUT
};

class MenuManager {
private:
  AppScreen currentScreen = SCREEN_MAIN;
  uint8_t selectedItem = 0;
  uint8_t previousSelectedItem = 0;
  bool screenChanged = true;
  bool selectionChanged = false;

  void changeScreen(AppScreen newScreen) {
    currentScreen = newScreen;
    selectedItem = 0;
    previousSelectedItem = 0;
    selectionChanged = false;
    screenChanged = true;
  }

  void returnToMainMenu() {
    changeScreen(SCREEN_MAIN_MENU);
  }

  void moveSelection(uint8_t newSelection) {
    if (newSelection == selectedItem) return;

    previousSelectedItem = selectedItem;
    selectedItem = newSelection;
    selectionChanged = true;
  }

public:
  void begin() {
    currentScreen = SCREEN_MAIN;
    selectedItem = 0;
    previousSelectedItem = 0;
    screenChanged = true;
    selectionChanged = false;
  }

  void handleEvent(ButtonEvent event) {
    // Long press ENTER: return to the previous menu.
    if (event.type == BTN_EVENT_LONG && event.button == BTN_ENTER) {
      if (currentScreen == SCREEN_MAIN_MENU) {
        changeScreen(SCREEN_MAIN);
      } else if (currentScreen != SCREEN_MAIN) {
        returnToMainMenu();
      }
      return;
    }

    // Only short presses navigate menus.
    if (event.type != BTN_EVENT_SHORT) return;

    if (currentScreen == SCREEN_MAIN) {
      if (event.button == BTN_ENTER) {
        changeScreen(SCREEN_MAIN_MENU);
      }
      return;
    }

    uint8_t itemCount = 0;

    switch (currentScreen) {
      case SCREEN_MAIN_MENU:
        itemCount = 4;
        break;
      case SCREEN_CALIBRATION_MENU:
        itemCount = 4;
        break;
      case SCREEN_SETTINGS_MENU:
        itemCount = 2;
        break;
      case SCREEN_DISPLAY_SETTINGS:
        itemCount = 3;
        break;
      default:
        return;
    }

    if (event.button == BTN_UP) {
      uint8_t next = (selectedItem == 0)
                       ? itemCount - 1
                       : selectedItem - 1;
      moveSelection(next);
    } else if (event.button == BTN_DOWN) {
      moveSelection((selectedItem + 1) % itemCount);
    } else if (event.button == BTN_ENTER &&
               currentScreen == SCREEN_MAIN_MENU) {
      switch (selectedItem) {
        case 0:
          changeScreen(SCREEN_CALIBRATION_MENU);
          break;
        case 1:
          changeScreen(SCREEN_SETTINGS_MENU);
          break;
        case 2:
          changeScreen(SCREEN_SENSOR_STATUS);
          break;
        case 3:
          changeScreen(SCREEN_ABOUT);
          break;
      }
    } else if (event.button == BTN_ENTER && currentScreen == SCREEN_SETTINGS_MENU) {
      if (selectedItem == 0) changeScreen(SCREEN_DISPLAY_SETTINGS);
    }
  }

  AppScreen getScreen() const {
    return currentScreen;
  }

  uint8_t getSelectedItem() const {
    return selectedItem;
  }

  uint8_t getPreviousSelectedItem() const {
    return previousSelectedItem;
  }

  bool hasScreenChanged() const {
    return screenChanged;
  }

  void clearScreenChanged() {
    screenChanged = false;
  }

  bool hasSelectionChanged() const {
    return selectionChanged;
  }

  void clearSelectionChanged() {
    selectionChanged = false;
  }
};

#endif
