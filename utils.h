#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>
#include "logger.h"

void blinkLED(int times);
void logMessage(const String &msg);
void logMessage(LogLevel level, const String &msg);
String getFormattedTime();

#endif