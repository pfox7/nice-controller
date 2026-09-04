#ifndef SETTINGS_H
#define SETTINGS_H

#include "config.h"
#include <Preferences.h>
#include <EEPROM.h>

struct BondedDevice {
  char address[18];
  char name[33];
  char pin[PIN_CODE_LENGTH + 1];
  bool active;
};

struct Settings {
  char sta_ssid[33];
  char sta_password[65];
  char mqtt_server[65];
  uint16_t mqtt_port;
  char mqtt_user[33];
  char mqtt_password[33];
  char mqtt_topic[65];
  char device_name[33];
  uint8_t wifi_timeout;
  float current_threshold;
  bool mqtt_tls_enabled;
  bool mqtt_verify_depth;
  bool ca_cert_present;
  bool client_cert_present;
  bool client_key_present;
  bool ble_enabled;
  bool ble_hid_mode;
  bool mqtt_retain;
  int8_t timezone;
};

extern Settings settings;
extern BondedDevice bondedDevices[MAX_BONDED_DEVICES];
extern int bondedCount;

void initSettings();
void resetSettings();
void saveSettings();
bool validateSettings();
void loadBondedDevices();
void saveBondedDevices();
int findBondedDevice(String address);
int addBondedDevice(String address, String name, String pin);
bool removeBondedDevice(String address);
String generatePin();

#endif