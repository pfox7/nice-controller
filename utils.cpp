#include "utils.h"
#include "config.h"
#include "time_manager.h"

void blinkLED(int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
    delay(200);
  }
}

void logMessage(const String &msg) {
  String line = TimeManager::getFormattedTime() + " | " + msg;
  Serial.println(line);
  logWrite(LOG_INFO, line);
}

void logMessage(LogLevel level, const String &msg) {
  String line = TimeManager::getFormattedTime() + " | " + msg;
  Serial.println(line);
  logWrite(level, line);
}

String getFormattedTime() {
  return TimeManager::getFormattedTime();
}