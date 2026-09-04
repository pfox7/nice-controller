#include "current_sensor.h"
#include "utils.h"

static float vref = 2.50;

void currentSensorSetup() {
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  pinMode(CURRENT_PIN, INPUT);

  // Ждём, чтобы двигатель гарантированно был остановлен
  delay(500);

  // Калибровка: усреднение 500 измерений для точности
  long sum = 0;
  const int samples = 500;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(CURRENT_PIN);
    delayMicroseconds(200);
  }
  float avgRaw = (float)sum / samples;
  vref = avgRaw * (3.3f / 4095.0f);

  logMessage("Датчик тока: калибровка, Vref=" + String(vref, 3) + " В");
  Serial.printf("Отладочное Vref: %.3f В\n", vref);
}

float readCurrent() {
  // Медианный фильтр: берём 101 измерение, отбрасываем крайние
  const int samples = 101;
  int raw[samples];
  long sum = 0;
  for (int i = 0; i < samples; i++) {
    raw[i] = analogRead(CURRENT_PIN);
    delayMicroseconds(100);
  }

  // Сортировка для медианы
  for (int i = 0; i < samples - 1; i++) {
    for (int j = i + 1; j < samples; j++) {
      if (raw[i] > raw[j]) {
        int tmp = raw[i];
        raw[i] = raw[j];
        raw[j] = tmp;
      }
    }
  }

  // Берём среднее из середины (от 25 до 75)
  for (int i = 25; i < 75; i++) {
    sum += raw[i];
  }
  float avgRaw = (float)sum / 50;
  float voltage = avgRaw * (3.3f / 4095.0f);
  float current = (voltage - vref) / 0.185f;  // ACS712-5A

  Serial.printf("Raw avg=%d, V=%.3f, I=%.2f A\n", (int)avgRaw, voltage, current);

  return abs(current);
}