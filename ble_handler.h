#ifndef BLE_HANDLER_H
#define BLE_HANDLER_H

#include "config.h"
#include "settings.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

extern bool bleDeviceConnected;

void initBLE();
void stopBLE();
void handleBLE();

#endif