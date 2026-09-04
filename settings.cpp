#include "settings.h"
#include "utils.h"
#include <LittleFS.h>

Settings settings;
BondedDevice bondedDevices[MAX_BONDED_DEVICES];
int bondedCount = 0;

static Preferences prefs;

bool validateSettings() {
  if (settings.device_name[0] == '\0' || strlen(settings.device_name) > 32) return false;
  if (settings.mqtt_port > 65535) return false;
  if (strlen(settings.mqtt_server) > 64) return false;
  if (settings.current_threshold < 0.1f || settings.current_threshold > 20.0f) return false;
  if (settings.wifi_timeout > 60) return false;
  if (settings.timezone < -12 || settings.timezone > 12) return false;
  if (settings.current_sensor_type > 2) return false;
  return true;
}

void initSettings() {
  prefs.begin("gate", false);
  settings.timezone = prefs.getChar("tz", 3);
  settings.mqtt_port = prefs.getUShort("mqtt_port", 1883);
  settings.current_threshold = prefs.getFloat("cur_thr", DEFAULT_THRESHOLD);
  settings.wifi_timeout = prefs.getUChar("wifi_to", 5);
  settings.ble_enabled = prefs.getBool("ble_en", false);
  settings.ble_hid_mode = prefs.getBool("ble_hid", false);
  settings.mqtt_retain = prefs.getBool("mqtt_ret", false);
  settings.current_sensor_type = prefs.getUChar("cur_sens", 0);

  String val;
  val = prefs.getString("sta_ssid", "");
  strncpy(settings.sta_ssid, val.c_str(), sizeof(settings.sta_ssid)-1);
  settings.sta_ssid[sizeof(settings.sta_ssid)-1] = '\0';

  val = prefs.getString("sta_pass", "");
  strncpy(settings.sta_password, val.c_str(), sizeof(settings.sta_password)-1);
  settings.sta_password[sizeof(settings.sta_password)-1] = '\0';

  val = prefs.getString("mqtt_srv", "");
  strncpy(settings.mqtt_server, val.c_str(), sizeof(settings.mqtt_server)-1);
  settings.mqtt_server[sizeof(settings.mqtt_server)-1] = '\0';

  val = prefs.getString("mqtt_user", "");
  strncpy(settings.mqtt_user, val.c_str(), sizeof(settings.mqtt_user)-1);
  settings.mqtt_user[sizeof(settings.mqtt_user)-1] = '\0';

  val = prefs.getString("mqtt_pass", "");
  strncpy(settings.mqtt_password, val.c_str(), sizeof(settings.mqtt_password)-1);
  settings.mqtt_password[sizeof(settings.mqtt_password)-1] = '\0';

  val = prefs.getString("mqtt_topic", "gate");
  strncpy(settings.mqtt_topic, val.c_str(), sizeof(settings.mqtt_topic)-1);
  settings.mqtt_topic[sizeof(settings.mqtt_topic)-1] = '\0';

  val = prefs.getString("dev_name", "GateController");
  strncpy(settings.device_name, val.c_str(), sizeof(settings.device_name)-1);
  settings.device_name[sizeof(settings.device_name)-1] = '\0';

  prefs.end();

  if (settings.device_name[0] == '\0' || !validateSettings()) {
    resetSettings();
    saveSettings();
  }

  logMessage("Настройки загружены");
}

void resetSettings() {
  memset(&settings, 0, sizeof(Settings));
  strcpy(settings.device_name, "GateController");
  strcpy(settings.mqtt_server, "");
  settings.mqtt_port = 1883;
  settings.wifi_timeout = 5;
  settings.current_threshold = DEFAULT_THRESHOLD;
  strcpy(settings.mqtt_topic, "gate");
  settings.timezone = 3;
  settings.current_sensor_type = 0;
}

void saveSettings() {
  prefs.begin("gate", false);
  prefs.putChar("tz", settings.timezone);
  prefs.putUShort("mqtt_port", settings.mqtt_port);
  prefs.putFloat("cur_thr", settings.current_threshold);
  prefs.putUChar("wifi_to", settings.wifi_timeout);
  prefs.putBool("ble_en", settings.ble_enabled);
  prefs.putBool("ble_hid", settings.ble_hid_mode);
  prefs.putBool("mqtt_ret", settings.mqtt_retain);
  prefs.putUChar("cur_sens", settings.current_sensor_type);
  prefs.putString("sta_ssid", settings.sta_ssid);
  prefs.putString("sta_pass", settings.sta_password);
  prefs.putString("mqtt_srv", settings.mqtt_server);
  prefs.putString("mqtt_user", settings.mqtt_user);
  prefs.putString("mqtt_pass", settings.mqtt_password);
  prefs.putString("mqtt_topic", settings.mqtt_topic);
  prefs.putString("dev_name", settings.device_name);
  prefs.end();
  logMessage("Настройки сохранены");
}

void loadBondedDevices() {
  EEPROM.begin(256);
  int addr = 0;
  EEPROM.get(addr, bondedCount);
  addr += sizeof(int);
  for (int i = 0; i < MAX_BONDED_DEVICES; i++) {
    EEPROM.get(addr, bondedDevices[i]);
    addr += sizeof(BondedDevice);
  }
  EEPROM.end();
}

void saveBondedDevices() {
  EEPROM.begin(256);
  int addr = 0;
  EEPROM.put(addr, bondedCount);
  addr += sizeof(int);
  for (int i = 0; i < MAX_BONDED_DEVICES; i++) {
    EEPROM.put(addr, bondedDevices[i]);
    addr += sizeof(BondedDevice);
  }
  EEPROM.commit();
  EEPROM.end();
}

int findBondedDevice(String address) {
  for (int i = 0; i < MAX_BONDED_DEVICES; i++) {
    if (bondedDevices[i].active && String(bondedDevices[i].address) == address) return i;
  }
  return -1;
}

int addBondedDevice(String address, String name, String pin) {
  int idx = findBondedDevice(address);
  if (idx >= 0) {
    bondedDevices[idx].active = true;
    name.toCharArray(bondedDevices[idx].name, 33);
    pin.toCharArray(bondedDevices[idx].pin, PIN_CODE_LENGTH + 1);
    saveBondedDevices();
    return idx;
  }
  for (int i = 0; i < MAX_BONDED_DEVICES; i++) {
    if (!bondedDevices[i].active) {
      address.toCharArray(bondedDevices[i].address, 18);
      name.toCharArray(bondedDevices[i].name, 33);
      pin.toCharArray(bondedDevices[i].pin, PIN_CODE_LENGTH + 1);
      bondedDevices[i].active = true;
      bondedCount++;
      saveBondedDevices();
      return i;
    }
  }
  return -1;
}

bool removeBondedDevice(String address) {
  for (int i = 0; i < MAX_BONDED_DEVICES; i++) {
    if (bondedDevices[i].active && String(bondedDevices[i].address) == address) {
      bondedDevices[i].active = false;
      bondedCount--;
      saveBondedDevices();
      return true;
    }
  }
  return false;
}

String generatePin() {
  String pin = "";
  for (int i = 0; i < PIN_CODE_LENGTH; i++) pin += String(random(0, 10));
  return pin;
}