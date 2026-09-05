#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============ ВЕРСИЯ ============
#define FIRMWARE_VERSION "2.7"   // Добавлен управляющий пин (кнопка)

// ============ ПИНЫ ============
#define SSR_MAIN_PIN      25   // D3 — фаза на двигатель
#define RELAY_CAP_PIN     16   // D5 — рабочий конденсатор
#define START_RELAY_PIN   13   // D9 — пусковой конденсатор

#define FCA_PIN           14   // D7
#define FCC_PIN           27   // D6
#define CURRENT_PIN       34   // A4
#define LED_PIN           2

// Управляющий пин (кнопка/переключатель)
#define BUTTON_PIN        12   // D11 — подача GND активирует
#define BUTTON_ACTIVE_STATE LOW // активный уровень - LOW

// Логика
#define SSR_ON            HIGH
#define SSR_OFF           LOW
#define RELAY_ON          LOW
#define RELAY_OFF         HIGH

// Параметры
#define DEFAULT_THRESHOLD 6.0f
#define DEFAULT_SENSITIVITY 0.185f   // ACS712-5A
#define START_DELAY_MS    300    // защита по току включается через 300 мс
#define OBSTACLE_BACK_MS  1500
#define MAX_RUN_TIME_MS   30000
#define START_CURRENT_THRESH 2.0f
#define START_MAX_TIME    3000

// Wi-Fi
#define AP_SSID_PREFIX    "Gate_"
#define AP_PASSWORD       "12345678"
#define WIFI_CONNECT_ATTEMPTS 20

// MQTT
#define MQTT_RECONNECT_INTERVAL 15000
#define MQTT_KEEPALIVE    60
#define MQTT_STATUS_INTERVAL 60000
#define MQTT_CURRENT_INTERVAL 10000

// EEPROM
#define EEPROM_SIZE       512
#define SIGNATURE_ADDR    0
#define SETTINGS_ADDR     4

// BLE
#ifndef SKIP_BLE
#define BLE_SERVICE_UUID            "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define BLE_CHARACTERISTIC_UUID_RX  "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define BLE_CHARACTERISTIC_UUID_TX  "beb5483e-36e1-4688-b7f5-ea07361b26a9"
#define BLE_CHARACTERISTIC_UUID_AUTH "beb5483e-36e1-4688-b7f5-ea07361b26aa"
#define MAX_BONDED_DEVICES          10
#define PIN_CODE_LENGTH             6
#endif

#endif