
#ifndef CONFIG_H
#define CONFIG_H

// ================================================
// DiversSI Tx Analyzer
// TTGO T4 V1.3 - 2.4"
// Configuration V0.3
// ================================================

// Project information
#define PROJECT_NAME "DiversSI Tx Analyzer"
#define PROJECT_VERSION "0.3"

// Serial communication
#define SERIAL_BAUDRATE 115200

// I2C configuration
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

// ADS1115 configuration
#define ADS1115_ADDRESS 0x48

// ADC gain
// GAIN_FOUR = +/- 1.024 V
#define ADS_GAIN_SETTING GAIN_FOUR

// Display configuration
#define DISPLAY_WIDTH 320
#define DISPLAY_HEIGHT 240

// Button configuration
#define BUTTON_UP 37
#define BUTTON_DOWN 38
#define BUTTON_ENTER 39

// Button logic
#define BUTTON_PRESSED LOW

#endif
