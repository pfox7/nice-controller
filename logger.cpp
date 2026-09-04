#include "logger.h"

static File logFile;
static LogLevel currentLevel = LOG_DEBUG;

void logInit() {
  if (!LittleFS.exists(LOG_FILE)) {
    logFile = LittleFS.open(LOG_FILE, "w");
    if (logFile) {
      logFile.println("=== Лог запущен ===");
      logFile.close();
    }
  }
}

void logWrite(LogLevel level, const String &message) {
  if (level < currentLevel) return;
  
  if (!logFile || !logFile.available()) {
    if (logFile) logFile.close();
    logFile = LittleFS.open(LOG_FILE, "a");
    if (!logFile) return;
  }
  
  String levelStr;
  switch (level) {
    case LOG_DEBUG: levelStr = "DEBUG"; break;
    case LOG_INFO:  levelStr = "INFO "; break;
    case LOG_WARN:  levelStr = "WARN "; break;
    case LOG_ERROR: levelStr = "ERROR"; break;
  }
  
  String line = levelStr + " | " + message;
  logFile.println(line);
  logFile.flush();
  
  if (logFile.size() > LOG_MAX_SIZE) {
    logFile.close();
    File fRead = LittleFS.open(LOG_FILE, "r");
    if (!fRead) return;
    String content = fRead.readString();
    fRead.close();
    int excess = content.length() - (LOG_MAX_SIZE * 0.9);
    if (excess > 0) {
      content = content.substring(excess);
      int nl = content.indexOf('\n');
      if (nl > 0) content = content.substring(nl + 1);
    }
    logFile = LittleFS.open(LOG_FILE, "w");
    if (logFile) {
      logFile.print(content);
      logFile.close();
    }
    logFile = LittleFS.open(LOG_FILE, "a");
  }
}

void logWrite(const String &message) {
  logWrite(LOG_INFO, message);
}

String logRead() {
  if (logFile) logFile.close();
  File f = LittleFS.open(LOG_FILE, "r");
  if (!f) return "Лог недоступен";
  String s = f.readString();
  f.close();
  return s;
}

void logClear() {
  if (logFile) logFile.close();
  LittleFS.remove(LOG_FILE);
  logInit();
}

void setLogLevel(LogLevel level) {
  currentLevel = level;
}

LogLevel getLogLevel() {
  return currentLevel;
}