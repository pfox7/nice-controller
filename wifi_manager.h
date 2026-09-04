#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "config.h"
#include "settings.h"
#include <WiFi.h>
#include <DNSServer.h>

extern bool apMode;
extern unsigned long wifiStartTime;
extern DNSServer dnsServer;
extern char deviceId[13];
extern bool needRestart;
extern unsigned long lastReconnectAttempt;

void generateDeviceId();
String getDeviceId();
String getApSSID();
String scanWiFiNetworks();
void initWiFi();
void startWiFiConnect(const char* ssid, const char* pass);
void updateWiFiConnect();
String getWiFiConnectStatus();
void tryReconnectSta();
void updateStaReconnect();
void handleWiFiTimeout();

#endif