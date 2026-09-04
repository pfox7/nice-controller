#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include "config.h"
#include "settings.h"
#include <WiFi.h>
#include <PubSubClient.h>

extern PubSubClient* mqtt;

void mqttSetup();
void mqttLoop();
void connectMQTT();
void publishState(String state);
void publishPosition(String pos);
void publishCurrent(float current);
void mqttStatusUpdate();

#endif