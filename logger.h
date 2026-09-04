#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <LittleFS.h>

#define LOG_FILE "/system.log"
#define LOG_MAX_SIZE 51200  // 50 КБ

enum LogLevel {
  LOG_DEBUG = 0,
  LOG_INFO  = 1,
  LOG_WARN  = 2,
  LOG_ERROR = 3
};

void logInit();
void logWrite(LogLevel level, const String &message);
void logWrite(const String &message);
String logRead();
void logClear();
void setLogLevel(LogLevel level);
LogLevel getLogLevel();

#endif