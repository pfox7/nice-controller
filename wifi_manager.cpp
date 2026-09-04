#include "wifi_manager.h"
#include "utils.h"
#include "time_manager.h"
#include <vector>
#include <algorithm>
#include <esp_wifi.h>

bool apMode = true;
unsigned long wifiStartTime = 0;
DNSServer dnsServer;
char deviceId[13];

static bool connectInProgress = false;
static String connectSSID, connectPass;
static unsigned long connectTimeout = 0;
static String connectStatus = "idle";
static String connectIP = "";
static int connectAttempts = 0;
static const int MAX_ATTEMPTS = 3;

bool needRestart = false;
unsigned long lastReconnectAttempt = 0;
static bool staReconnectInProgress = false;
static unsigned long staReconnectStart = 0;
static const unsigned long STA_RECONNECT_INTERVAL = 60000; // 60 сек

// Обработчик событий Wi-Fi (совместим с ESP32 Arduino Core 3.x)
void WiFiEvent(arduino_event_id_t event, arduino_event_info_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_START:
      logMessage(LOG_DEBUG, "WiFi Event: STA запущен");
      break;
    case ARDUINO_EVENT_WIFI_STA_STOP:
      logMessage(LOG_DEBUG, "WiFi Event: STA остановлен");
      break;
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      logMessage(LOG_INFO, "WiFi Event: подключен к точке доступа, канал " + String(info.wifi_sta_connected.channel) +
                 ", BSSID: " + String(info.wifi_sta_connected.bssid[0], HEX) + ":" + String(info.wifi_sta_connected.bssid[1], HEX) + "...");
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      logMessage(LOG_WARN, "WiFi Event: отключен от точки доступа, причина " + String(info.wifi_sta_disconnected.reason));
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      logMessage(LOG_INFO, "WiFi Event: получен IP " + IPAddress(info.got_ip.ip_info.ip.addr).toString() +
                 ", маска " + IPAddress(info.got_ip.ip_info.netmask.addr).toString() +
                 ", шлюз " + IPAddress(info.got_ip.ip_info.gw.addr).toString());
      break;
    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
      logMessage(LOG_WARN, "WiFi Event: IP адрес потерян");
      break;
    case ARDUINO_EVENT_WIFI_AP_START:
      logMessage(LOG_INFO, "WiFi Event: точка доступа запущена");
      break;
    case ARDUINO_EVENT_WIFI_AP_STOP:
      logMessage(LOG_INFO, "WiFi Event: точка доступа остановлена");
      break;
    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
      logMessage(LOG_INFO, "WiFi Event: клиент подключился к AP");
      break;
    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
      logMessage(LOG_INFO, "WiFi Event: клиент отключился от AP");
      break;
    default:
      break;
  }
}

void generateDeviceId() {
  uint64_t chipId = ESP.getEfuseMac();
  snprintf(deviceId, 13, "%04X%08X", (uint16_t)(chipId >> 32), (uint32_t)chipId);
  logMessage(LOG_INFO, "Device ID: " + String(deviceId));
}

String getDeviceId() { return String(deviceId); }
String getApSSID() { return String(AP_SSID_PREFIX) + deviceId; }

String scanWiFiNetworks() {
  logMessage(LOG_DEBUG, "Wi-Fi: сканирование сетей...");
  int n = WiFi.scanNetworks();
  String json = "[";
  struct WiFiNetwork { String ssid; int rssi; String encryption; };
  std::vector<WiFiNetwork> networks;
  for (int i = 0; i < n; i++) {
    networks.push_back({WiFi.SSID(i), WiFi.RSSI(i), 
      WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "OPEN" : "SECURED"});
  }
  std::sort(networks.begin(), networks.end(), [](WiFiNetwork a, WiFiNetwork b) {
    return a.rssi > b.rssi;
  });
  bool first = true;
  String lastSsid = "";
  for (size_t i = 0; i < networks.size(); i++) {
    if (networks[i].ssid == lastSsid) continue;
    lastSsid = networks[i].ssid;
    if (!first) json += ",";
    json += "{\"ssid\":\"" + networks[i].ssid + "\",";
    json += "\"rssi\":" + String(networks[i].rssi) + ",";
    json += "\"encryption\":\"" + networks[i].encryption + "\"}";
    first = false;
  }
  json += "]";
  WiFi.scanDelete();
  logMessage(LOG_DEBUG, "Wi-Fi: найдено " + String(n) + " сетей");
  return json;
}

void initWiFi() {
  generateDeviceId();
  
  // Регистрируем обработчик событий
  WiFi.onEvent(WiFiEvent);
  
  // Настройки для стабильности
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  
  if (strlen(settings.sta_ssid) > 0) {
    apMode = false;
    logMessage(LOG_INFO, "Wi-Fi: подключение к " + String(settings.sta_ssid));
    WiFi.mode(WIFI_STA);
    WiFi.begin(settings.sta_ssid, settings.sta_password);
    wifiStartTime = millis();
    int attempts = WIFI_CONNECT_ATTEMPTS;
    while (WiFi.status() != WL_CONNECTED && attempts-- > 0) {
      delay(500);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      logMessage(LOG_INFO, "Wi-Fi: подключён! IP: " + WiFi.localIP().toString() +
                 ", RSSI: " + String(WiFi.RSSI()) + " dBm");
      TimeManager::syncNTP();
    } else {
      logMessage(LOG_WARN, "Wi-Fi: сеть не найдена, запуск точки доступа");
      apMode = true;
    }
  } else {
    logMessage(LOG_INFO, "Wi-Fi: нет сохранённой сети, запуск точки доступа");
    apMode = true;
  }
  
  if (apMode) {
    WiFi.mode(WIFI_AP);
    String apSsid = getApSSID();
    WiFi.softAP(apSsid.c_str(), AP_PASSWORD);
    dnsServer.start(53, "*", WiFi.softAPIP());
    logMessage(LOG_INFO, "Wi-Fi: точка доступа запущена. SSID: " + apSsid);
  }
}

void startWiFiConnect(const char* ssid, const char* pass) {
  connectSSID = ssid;
  connectPass = pass;
  connectAttempts = 0;
  connectTimeout = millis() + 60000;
  connectStatus = "connecting";
  connectIP = "";
  WiFi.softAPdisconnect(true);
  delay(100);
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  delay(200);
  WiFi.begin(ssid, pass);
  logMessage(LOG_INFO, "Асинхронное подключение к " + String(ssid) + " запущено (попыток: " + String(MAX_ATTEMPTS) + ")");
  connectInProgress = true;
}

void updateWiFiConnect() {
  if (!connectInProgress) return;
  wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED) {
    connectStatus = "connected";
    connectIP = WiFi.localIP().toString();
    connectInProgress = false;
    logMessage(LOG_INFO, "Асинхронное подключение успешно, IP: " + connectIP +
               ", RSSI: " + String(WiFi.RSSI()) + " dBm");
    apMode = false;
    strncpy(settings.sta_ssid, connectSSID.c_str(), sizeof(settings.sta_ssid)-1);
    settings.sta_ssid[sizeof(settings.sta_ssid)-1] = '\0';
    strncpy(settings.sta_password, connectPass.c_str(), sizeof(settings.sta_password)-1);
    settings.sta_password[sizeof(settings.sta_password)-1] = '\0';
    saveSettings();
    needRestart = true;   // только при ручной настройке
    TimeManager::syncNTP();
  } else if (millis() > connectTimeout) {
    if (++connectAttempts < MAX_ATTEMPTS) {
      logMessage(LOG_WARN, "Попытка " + String(connectAttempts) + "/" + String(MAX_ATTEMPTS) +
                 " не удалась (статус: " + String(status) + "), повтор через 5 сек...");
      WiFi.disconnect(true);
      delay(5000);
      WiFi.begin(connectSSID.c_str(), connectPass.c_str());
      connectTimeout = millis() + 60000;
    } else {
      connectStatus = "failed";
      connectInProgress = false;
      logMessage(LOG_ERROR, "Асинхронное подключение провалилось после " + String(MAX_ATTEMPTS) +
                 " попыток. Статус: " + String(status));
      WiFi.disconnect(true);
      delay(200);
      WiFi.mode(WIFI_AP);
      delay(200);
      WiFi.softAP(getApSSID().c_str(), AP_PASSWORD);
      dnsServer.start(53, "*", WiFi.softAPIP());
      apMode = true;
      logMessage(LOG_INFO, "Точка доступа восстановлена");
    }
  }
}

String getWiFiConnectStatus() {
  String json = "{\"status\":\"" + connectStatus + "\"";
  if (connectStatus == "connected") {
    json += ",\"ip\":\"" + connectIP + "\"";
  }
  json += "}";
  return json;
}

void tryReconnectSta() {
  if (!apMode || staReconnectInProgress) return;
  if (strlen(settings.sta_ssid) == 0) return;
  if (millis() - lastReconnectAttempt < STA_RECONNECT_INTERVAL) return;
  lastReconnectAttempt = millis();
  
  logMessage(LOG_INFO, "Фоновая попытка подключения к " + String(settings.sta_ssid));
  WiFi.disconnect(false);
  delay(100);
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(settings.sta_ssid, settings.sta_password);
  staReconnectInProgress = true;
  staReconnectStart = millis();
}

void updateStaReconnect() {
  if (!staReconnectInProgress) return;
  if (WiFi.status() == WL_CONNECTED) {
    logMessage(LOG_INFO, "Фоновое подключение успешно, IP: " + WiFi.localIP().toString() +
               ", RSSI: " + String(WiFi.RSSI()) + " dBm");
    apMode = false;
    // НЕ перезагружаемся! Просто продолжаем работать
    staReconnectInProgress = false;
    // Сохраняем настройки на всякий случай
    saveSettings();
    // Обновляем таймер для таймаута
    wifiStartTime = millis();
    TimeManager::syncNTP();
  } else if (millis() - staReconnectStart > 30000) {
    logMessage(LOG_WARN, "Фоновая попытка не удалась");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(getApSSID().c_str(), AP_PASSWORD);
    dnsServer.start(53, "*", WiFi.softAPIP());
    staReconnectInProgress = false;
  }
}

void handleWiFiTimeout() {
  if (!apMode && WiFi.status() != WL_CONNECTED) {
    // Если уже идёт процесс подключения, не мешаем
    if (connectInProgress || staReconnectInProgress) return;
    
    unsigned long elapsed = (millis() - wifiStartTime) / 60000;
    if (elapsed >= settings.wifi_timeout) {
      logMessage(LOG_WARN, "Wi-Fi: таймаут ожидания, пытаюсь переподключиться...");
      WiFi.disconnect(false);
      delay(100);
      WiFi.mode(WIFI_AP_STA);
      WiFi.begin(settings.sta_ssid, settings.sta_password);
      staReconnectInProgress = true;
      staReconnectStart = millis();
      wifiStartTime = millis(); // сброс таймера
    }
  }
}