#include "time_manager.h"
#include <WiFi.h>

static int8_t currentTimezone = 3;
static bool ntpSynced = false;
static time_t baseTime = 0;

void TimeManager::begin() {
  struct tm tm = {};
  tm.tm_year = 2026 - 1900;
  tm.tm_mon = 6;
  tm.tm_mday = 17;
  tm.tm_hour = 0;
  tm.tm_min = 0;
  tm.tm_sec = 0;
  baseTime = mktime(&tm) - currentTimezone * 3600;
  if (WiFi.status() == WL_CONNECTED) {
    syncNTP();
  }
}

void TimeManager::setTimezone(int8_t tz) {
  if (tz >= -12 && tz <= 12) {
    currentTimezone = tz;
    if (!ntpSynced) {
      struct tm tm = {};
      tm.tm_year = 2026 - 1900;
      tm.tm_mon = 6;
      tm.tm_mday = 17;
      tm.tm_hour = 0;
      tm.tm_min = 0;
      tm.tm_sec = 0;
      baseTime = mktime(&tm) - currentTimezone * 3600;
    }
  }
}

int8_t TimeManager::getTimezone() {
  return currentTimezone;
}

String TimeManager::getFormattedTime() {
  time_t now;
  if (ntpSynced) {
    time(&now);
  } else {
    now = baseTime + (millis() / 1000);
  }
  struct tm *tm = localtime(&now);
  char buf[20];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm);
  return String(buf);
}

void TimeManager::syncNTP() {
  configTime(currentTimezone * 3600, 0, "pool.ntp.org", "time.nist.gov");
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 5000)) {
    ntpSynced = true;
  }
}