#include "settings.h"
#include "utils.h"
#include <LittleFS.h>

Settings settings;
BondedDevice bondedDevices[MAX_BONDED_DEVICES];
int bondedCount = 0;

static Preferences prefs;

bool validateSettings() {
  if (settings.device_name[0] == '\0' || strlen(settings.device_name) > 32) {
    logMessage("Настройки: некорректное имя устройства");
    return false;
  }
  for (size_t i = 0; i < strlen(settings.device_name); i++) {
    if (settings.device_name[i] < 32 || settings.device_name[i] > 126) {
      logMessage("Настройки: мусорные символы в имени устройства");
      return false;
    }
  }
  if (settings.mqtt_port > 65535) {
    logMessage("Настройки: некорректный порт MQTT");
    return false;
  }
  if (strlen(settings.mqtt_server) > 64) {
    logMessage("Настройки: некорректный сервер MQTT");
    return false;
  }
  if (settings.current_threshold < 0.1f || settings.current_threshold > 20.0f) {
    logMessage("Настройки: некорректный порог тока");
    return false;
  }
  if (settings.current_sensitivity < 0.01f || settings.current_sensitivity > 1.0f) {
    logMessage("Настройки: некорректная чувствительность датчика тока");
    return false;
  }
  if (settings.wifi_timeout > 60) {
    logMessage("Настройки: некорректный таймаут Wi-Fi");
    return false;
  }
  if (settings.timezone < -12 || settings.timezone > 12) {
    logMessage("Настройки: некорректная временная зона");
    return false;
  }
  for (int i = 0; i < 33 && settings.sta_ssid[i]; i++) {
    if (settings.sta_ssid[i] < 32 || settings.sta_ssid[i] > 126) {
      logMessage("Настройки: мусор в sta_ssid");
      return false;
    }
  }
  for (int i = 0; i < 65 && settings.sta_password[i]; i++) {
    if (settings.sta_password[i] < 32 || settings.sta_password[i] > 126) {
      logMessage("Настройки: мусор в sta_password");
      return false;
    }
  }
#ifdef ENABLE_TLS
  if (settings.mqtt_tls_enabled) {
    // Дополнительные проверки TLS
  }
#endif
  return true;
}

void initSettings() {
  prefs.begin("gate", false);
  
  String val;
  settings.timezone = prefs.getChar("tz", 3);
  settings.mqtt_port = prefs.getUShort("mqtt_port", 1883);
  settings.current_threshold = prefs.getFloat("cur_thr", DEFAULT_THRESHOLD);
  settings.current_sensitivity = prefs.getFloat("cur_sens", DEFAULT_SENSITIVITY);
  settings.wifi_timeout = prefs.getUChar("wifi_to", 5);
  
  settings.ble_enabled = prefs.getBool("ble_en", false);
  settings.ble_hid_mode = prefs.getBool("ble_hid", false);
  settings.mqtt_retain = prefs.getBool("mqtt_ret", false);
  
#ifdef ENABLE_TLS
  settings.mqtt_tls_enabled = prefs.getBool("tls_en", false);
  settings.mqtt_verify_depth = prefs.getBool("tls_vd", false);
  settings.ca_cert_present = prefs.getBool("ca_cert", false);
  settings.client_cert_present = prefs.getBool("cl_cert", false);
  settings.client_key_present = prefs.getBool("cl_key", false);
#endif
  
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
  
  if (settings.device_name[0] == '\0') {
    resetSettings();
    saveSettings();
  } else if (!validateSettings()) {
    resetSettings();
    saveSettings();
  }
  
  logMessage("Настройки загружены");
  logMessage("  Имя устройства: " + String(settings.device_name));
  logMessage("  Сервер MQTT: " + String(settings.mqtt_server));
  logMessage("  Порт MQTT: " + String(settings.mqtt_port));
  logMessage("  Топик MQTT: " + String(settings.mqtt_topic));
  logMessage("  Порог тока: " + String(settings.current_threshold, 1) + " А");
  logMessage("  Чувствительность датчика тока: " + String(settings.current_sensitivity, 3) + " В/А");
  logMessage("  Таймаут Wi-Fi: " + String(settings.wifi_timeout) + " мин");
  logMessage("  BLE: " + String(settings.ble_enabled ? "да" : "нет"));
  logMessage("  Retain: " + String(settings.mqtt_retain ? "да" : "нет"));
  logMessage("  Часовой пояс: UTC" + String(settings.timezone >= 0 ? "+" : "") + String(settings.timezone));
}

void resetSettings() {
  memset(&settings, 0, sizeof(Settings));
  strcpy(settings.device_name, "GateController");
  strcpy(settings.mqtt_server, "");
  settings.mqtt_port = 1883;
  settings.wifi_timeout = 5;
  settings.current_threshold = DEFAULT_THRESHOLD;
  settings.current_sensitivity = DEFAULT_SENSITIVITY;
  strcpy(settings.mqtt_topic, "gate");
  settings.timezone = 3;
  logMessage("Настройки сброшены на значения по умолчанию");
}

void saveSettings() {
  prefs.begin("gate", false);
  
  prefs.putChar("tz", settings.timezone);
  prefs.putUShort("mqtt_port", settings.mqtt_port);
  prefs.putFloat("cur_thr", settings.current_threshold);
  prefs.putFloat("cur_sens", settings.current_sensitivity);
  prefs.putUChar("wifi_to", settings.wifi_timeout);
  
  prefs.putBool("ble_en", settings.ble_enabled);
  prefs.putBool("ble_hid", settings.ble_hid_mode);
  prefs.putBool("mqtt_ret", settings.mqtt_retain);
  
#ifdef ENABLE_TLS
  prefs.putBool("tls_en", settings.mqtt_tls_enabled);
  prefs.putBool("tls_vd", settings.mqtt_verify_depth);
  prefs.putBool("ca_cert", settings.ca_cert_present);
  prefs.putBool("cl_cert", settings.client_cert_present);
  prefs.putBool("cl_key", settings.client_key_present);
#endif
  
  prefs.putString("sta_ssid", settings.sta_ssid);
  prefs.putString("sta_pass", settings.sta_password);
  prefs.putString("mqtt_srv", settings.mqtt_server);
  prefs.putString("mqtt_user", settings.mqtt_user);
  prefs.putString("mqtt_pass", settings.mqtt_password);
  prefs.putString("mqtt_topic", settings.mqtt_topic);
  prefs.putString("dev_name", settings.device_name);
  
  prefs.end();
  logMessage("Настройки сохранены в Preferences");
  logMessage("  Имя: " + String(settings.device_name) +
             ", MQTT: " + String(settings.mqtt_server) +
             ":" + String(settings.mqtt_port) +
             ", Топик: " + String(settings.mqtt_topic) +
             ", Порог тока: " + String(settings.current_threshold, 1) +
             ", Чувствительность: " + String(settings.current_sensitivity, 3) +
             ", Retain: " + String(settings.mqtt_retain ? "да" : "нет") +
             ", BLE: " + String(settings.ble_enabled ? "вкл" : "выкл") +
             ", Wi-Fi: " + String(settings.sta_ssid));
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
  logMessage("Привязанные устройства: загружено " + String(bondedCount));
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
  logMessage("Привязанные устройства сохранены");
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
    logMessage("Привязанное устройство обновлено: " + address);
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
      logMessage("Привязанное устройство добавлено: " + address);
      return i;
    }
  }
  logMessage("Привязанные устройства: список заполнен!");
  return -1;
}

bool removeBondedDevice(String address) {
  for (int i = 0; i < MAX_BONDED_DEVICES; i++) {
    if (bondedDevices[i].active && String(bondedDevices[i].address) == address) {
      bondedDevices[i].active = false;
      bondedCount--;
      saveBondedDevices();
      logMessage("Привязанное устройство удалено: " + address);
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