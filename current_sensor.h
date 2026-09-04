#ifndef CURRENT_SENSOR_H
#define CURRENT_SENSOR_H

#include "config.h"
#include <Arduino.h>

void currentSensorSetup();
float readCurrent();
void setCurrentSensorType(uint8_t type);
void recalibrateCurrentSensor();

#endif