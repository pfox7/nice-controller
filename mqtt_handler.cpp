#include "mqtt_handler.h"
#include "motor_control.h"
#include "wifi_manager.h"
#include "utils.h"
#include "time_manager.h"
#include "current_sensor.h"
#ifdef ENABLE_TLS
#include <WiFiClientSecure.h>
#endif
#include <LittleFS.h>

WiFiClient espClient;
#ifdef ENABLE_TLS
WiFiClientSecure* secureClient = nullptr;
#endif
PubSubClient* mqtt = nullptr;
static bool useTLS = false;

static unsigned long lastStatusTime = 0;
static unsigned long lastCurrentTime = 0;
static String lastPosition = "";

static bool isMqttConfigured() {
  return (strlen(settings.mqtt_server) > 0 && settings.mqtt_port > 0 && settings.mqtt_port <= 65535);
}

#ifdef ENABLE_TLS
static String loadCertFromSPIFFS(const char* path) {
  if (!LittleFS.exists(path)) return "";
  File f = LittleFS.open(path, "r");
  if (!f) return "";
  String cert = f.readString();
  f.close();
  return cert;
}
#endif

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) message += (char)payload[i];
  
  String topicStr = String(topic);
  String baseTopic = String(settings.mqtt_topic) + "/" + getDeviceId();
  
  if (topicStr == baseTopic + "/command") {
    if (message == "FORWARD") startForward();
    else if (message == "REVERSE") startReverse();
    else if (message == "STOP") stopMotor();
  } else if (topicStr == baseTopic + "/settings/threshold") {
    settings.current_threshold = message.toFloat();
    saveSettings();
  } else if (topicStr == baseTopic + "/settings/reboot") {
    ESP.restart();
  } else if (topicStr == baseTopic + "/settings/retain") {
    settings.mqtt_retain = (message == "1" || message == "true");
    saveSettings();
  } else if (topicStr == baseTopic + "/settings/timezone") {
    settings.timezone = message.toInt();
    TimeManager::setTimezone(settings.timezone);
    saveSettings();
  }
#ifdef ENABLE_TLS
  else if (topicStr == baseTopic + "/settings/tls/enable") {
    settings.mqtt_tls_enabled = (message == "1");
    saveSettings();
    connectMQTT();
  }
#endif
}

void mqttSetup() {
  logMessage("MQTT: инициализация...");
  mqtt = new PubSubClient(espClient);
  mqtt->setKeepAlive(MQTT_KEEPALIVE);
  if (isMqttConfigured() && WiFi.status() == WL_CONNECTED) {
    connectMQTT();
  }
}

void mqttLoop() {
  if (mqtt == nullptr) return;
  if (!isMqttConfigured() || WiFi.status() != WL_CONNECTED) return;
  
  if (!mqtt->connected()) {
    static unsigned long lastReconnect = 0;
    if (millis() - lastReconnect > MQTT_RECONNECT_INTERVAL) {
      connectMQTT();
      lastReconnect = millis();
    }
  } else {
    mqtt->loop();
    
    if (millis() - lastStatusTime >= MQTT_STATUS_INTERVAL) {
      lastStatusTime = millis();
      String baseTopic = String(settings.mqtt_topic) + "/" + getDeviceId();
      mqtt->publish((baseTopic + "/status").c_str(), "online", settings.mqtt_retain);
      logMessage("Периодический статус: online");
    }
    
    if (millis() - lastCurrentTime >= MQTT_CURRENT_INTERVAL) {
      lastCurrentTime = millis();
      float current = readCurrent();
      publishCurrent(current);
    }
    
    String pos = getPositionString();
    if (pos != lastPosition) {
      lastPosition = pos;
      publishPosition(pos);
    }
  }
}

void connectMQTT() {
  if (mqtt == nullptr || !isMqttConfigured() || WiFi.status() != WL_CONNECTED) return;
  
  mqtt->disconnect();
  
#ifdef ENABLE_TLS
  if (settings.mqtt_tls_enabled && settings.ca_cert_present) {
    if (secureClient == nullptr) secureClient = new WiFiClientSecure();
    String ca = loadCertFromSPIFFS("/ca.crt");
    if (ca.length() > 0) secureClient->setCACert(ca.c_str());
    if (settings.client_cert_present && settings.client_key_present) {
      String cert = loadCertFromSPIFFS("/client.crt");
      String key = loadCertFromSPIFFS("/client.key");
      if (cert.length() > 0 && key.length() > 0) {
        secureClient->setCertificate(cert.c_str());
        secureClient->setPrivateKey(key.c_str());
      }
    }
    mqtt->setClient(*secureClient);
  } else {
    mqtt->setClient(espClient);
  }
#else
  mqtt->setClient(espClient);
#endif
  
  mqtt->setServer(settings.mqtt_server, settings.mqtt_port);
  mqtt->setCallback(mqttCallback);
  
  String clientId = String(settings.device_name) + "_" + getDeviceId();
  if (mqtt->connect(clientId.c_str(), settings.mqtt_user, settings.mqtt_password)) {
    logMessage("MQTT подключён");
    String baseTopic = String(settings.mqtt_topic) + "/" + getDeviceId();
    mqtt->subscribe((baseTopic + "/command").c_str());
    mqtt->subscribe((baseTopic + "/settings/#").c_str());
    mqtt->publish((baseTopic + "/status").c_str(), "online", settings.mqtt_retain);
    lastStatusTime = millis();
  } else {
    logMessage("MQTT ошибка подключения: " + String(mqtt->state()));
  }
}

void publishState(String state) {
  if (mqtt == nullptr || !mqtt->connected()) return;
  String baseTopic = String(settings.mqtt_topic) + "/" + getDeviceId();
  mqtt->publish((baseTopic + "/state").c_str(), state.c_str(), settings.mqtt_retain);
}

void publishPosition(String pos) {
  if (mqtt == nullptr || !mqtt->connected()) return;
  String baseTopic = String(settings.mqtt_topic) + "/" + getDeviceId();
  mqtt->publish((baseTopic + "/position").c_str(), pos.c_str(), settings.mqtt_retain);
}

void publishCurrent(float current) {
  if (mqtt == nullptr || !mqtt->connected()) return;
  String baseTopic = String(settings.mqtt_topic) + "/" + getDeviceId();
  mqtt->publish((baseTopic + "/current").c_str(), String(current, 2).c_str());
}

void mqttStatusUpdate() {
  if (mqtt == nullptr || !mqtt->connected()) return;
  String baseTopic = String(settings.mqtt_topic) + "/" + getDeviceId();
  mqtt->publish((baseTopic + "/status").c_str(), "online", settings.mqtt_retain);
  lastStatusTime = millis();
}