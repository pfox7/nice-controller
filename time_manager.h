#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <Arduino.h>
#include <time.h>

class TimeManager {
public:
  static void begin();
  static void setTimezone(int8_t tz);
  static int8_t getTimezone();
  static String getFormattedTime();
  static void syncNTP();
};

#endif