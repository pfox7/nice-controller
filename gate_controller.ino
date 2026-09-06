#include "config.h"
#include "settings.h"
#include "wifi_manager.h"
#include "motor_control.h"
#include "current_sensor.h"
#include "mqtt_handler.h"
#include "ble_handler.h"
#include "web_server.h"
#include "utils.h"
#include "logger.h"
#include "time_manager.h"
#include <LittleFS.h>
#include <ESPmDNS.h>

void listLittleFS() {
  logMessage("=== Содержимое LittleFS ===");
  if (!LittleFS.begin(false)) {
    logMessage("LittleFS не смонтирована!");
    return;
  }
  File root = LittleFS.open("/");
  if (!root) {
    logMessage("Не удалось открыть корень LittleFS");
    return;
  }
  File file = root.openNextFile();
  if (!file) {
    logMessage("LittleFS пуста");
    return;
  }
  while (file) {
    logMessage("  " + String(file.name()) + "  (" + String(file.size()) + " байт)");
    file = root.openNextFile();
  }
  logMessage("=========================");
}

void ensureIndexHTML() {
  if (LittleFS.exists("/index.html")) {
    File f = LittleFS.open("/index.html", "r");
    if (f && f.size() > 100) {
      logMessage("index.html найден в LittleFS, размер " + String(f.size()) + " байт");
      f.close();
      return;
    }
    if (f) f.close();
    logMessage("index.html повреждён или пуст, будет создан минимальный");
  } else {
    logMessage("index.html отсутствует в LittleFS, создаю минимальный");
  }

  File f = LittleFS.open("/index.html", "w");
  if (f) {
    f.print(getMinimalHTML());
    f.close();
    logMessage("Минимальный index.html записан в LittleFS");
  } else {
    logMessage("Ошибка создания index.html");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  logInit();
  logMessage("=== ЗАПУСК КОНТРОЛЛЕРА v" + String(FIRMWARE_VERSION) + " ===");

  Serial.println();
  Serial.println("╔══════════════════════════════════════╗");
  Serial.println("║     GATE CONTROLLER v" + String(FIRMWARE_VERSION) + "             ║");
  Serial.println("╚══════════════════════════════════════╝");
  Serial.println();

  TimeManager::begin();
  TimeManager::setTimezone(settings.timezone);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Инициализация управляющего пина (кнопка)
  pinMode(BUTTON_PIN, INPUT_PULLUP);   // <-- добавлено

  logMessage("Инициализация двигателя");
  motorSetup();

  logMessage("Инициализация датчика тока");
  currentSensorSetup();

  logMessage("Монтирование LittleFS");
  if (!LittleFS.begin(true)) {
    logMessage("LittleFS: ошибка монтирования, форматирование...");
    if (!LittleFS.format()) {
      logMessage("LittleFS: фатальная ошибка, перезагрузка");
      ESP.restart();
    }
    if (!LittleFS.begin(true)) {
      logMessage("LittleFS: не удалось смонтировать после форматирования");
      ESP.restart();
    }
  }
  logMessage("LittleFS смонтирована");
  listLittleFS();

  logMessage("Загрузка настроек");
  initSettings();
  loadBondedDevices();
  TimeManager::setTimezone(settings.timezone);

  logMessage("Проверка/создание index.html");
  ensureIndexHTML();

  logMessage("Инициализация Wi-Fi");
  initWiFi();

  if (!apMode) {
    if (MDNS.begin(settings.device_name)) {
      delay(200);
      logMessage("mDNS запущен: http://" + String(settings.device_name) + ".local");
    } else {
      logMessage("Ошибка запуска mDNS");
    }
    TimeManager::syncNTP();
  }

  logMessage("Запуск веб-сервера");
  initWebServer();

  logMessage("Запуск MQTT");
  mqttSetup();

  logMessage("Запуск BLE");
  if (settings.ble_enabled) {
    initBLE();
  } else {
    logMessage("BLE отключен в настройках");
  }

  logMessage("Система готова");
  if (!apMode) {
    logMessage("IP адрес: " + WiFi.localIP().toString());
  } else {
    logMessage("Точка доступа: " + getApSSID());
  }

  blinkLED(2);
}

void loop() {
  // Обработка кнопки (всегда)
  handleButton();   // <-- добавлено

  bool justStarted = (currentState == MOVING_FORWARD || currentState == MOVING_REVERSE) &&
                     (millis() - moveStartTime < 50);

  if (!justStarted) {
    if (apMode) {
      dnsServer.processNextRequest();
      tryReconnectSta();
    }

    server.handleClient();
    updateWiFiConnect();
    updateStaReconnect();

    if (!apMode && WiFi.status() == WL_CONNECTED) {
      mqttLoop();
    }
  }

  if (!justStarted) {
    handleStartRelay();
  }

  if (needRestart) {
    logMessage("Перезагрузка...");
    delay(2000);
    ESP.restart();
  }

  if (!apMode && !justStarted) {
    if (WiFi.status() != WL_CONNECTED) {
      handleWiFiTimeout();
    }
  }

  // Надёжная проверка концевиков с тройным чтением
  if (!testMode) {
    if (currentState == MOVING_FORWARD || currentState == MOVING_REVERSE) {
      int fcaHighCount = 0;
      int fccHighCount = 0;
      for (int i = 0; i < 3; i++) {
        if (digitalRead(FCA_PIN) == HIGH) fcaHighCount++;
        if (digitalRead(FCC_PIN) == HIGH) fccHighCount++;
        delay(10);
      }

      if (currentState == MOVING_FORWARD && fcaHighCount >= 3) {
        logMessage("Двигатель: достигнут концевик открытия");
        stopMotor();
      } else if (currentState == MOVING_REVERSE && fccHighCount >= 3) {
        logMessage("Двигатель: достигнут концевик закрытия");
        stopMotor();
      }
    }

    if (currentState == MOVING_FORWARD || currentState == MOVING_REVERSE) {
      if (millis() - moveStartTime > MAX_RUN_TIME_MS) {
        logMessage("Двигатель: превышено время работы, остановка");
        stopMotor();
      }

      // Проверка тока после 300 мс от старта
      if (millis() - moveStartTime > 300) {
        float current = readCurrent();
        if (current > settings.current_threshold) {
          logMessage("Двигатель: ПРЕПЯТСТВИЕ! Ток: " + String(current, 2) + " А");
          String dir = (currentState == MOVING_FORWARD) ? "FORWARD" : "REVERSE";
          handleObstacle(dir);
        }
      }
    }

    if (currentState == OBSTACLE_BACKWARD) {
      if (millis() - moveStartTime > OBSTACLE_BACK_MS) {
        logMessage("Двигатель: откат завершён");
        stopMotor();
      }
    }
  }

  delay(30);
}