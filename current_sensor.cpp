#include "current_sensor.h"
#include "settings.h"
#include "utils.h"

static float vref = 2.50;
static float sensitivity = 0.185;

void setCurrentSensorType(uint8_t type) {
  switch (type) {
    case 0: sensitivity = 0.185; break;
    case 1: sensitivity = 0.100; break;
    case 2: sensitivity = 0.066; break;
    default: sensitivity = 0.185;
  }
  settings.current_sensor_type = type;
  saveSettings();
  recalibrateCurrentSensor();
}

void recalibrateCurrentSensor() {
  long sum = 0;
  const int samples = 500;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(CURRENT_PIN);
    delayMicroseconds(200);
  }
  vref = (sum / (float)samples) * (3.3f / 4095.0f);
  logMessage("Датчик тока: калибровка, Vref=" + String(vref, 3) + " В");
}

void currentSensorSetup() {
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  pinMode(CURRENT_PIN, INPUT);
  setCurrentSensorType(settings.current_sensor_type);
  recalibrateCurrentSensor();
}

float readCurrent() {
  const int samples = 101;
  int raw[samples];
  for (int i = 0; i < samples; i++) {
    raw[i] = analogRead(CURRENT_PIN);
    delayMicroseconds(100);
  }
  for (int i = 0; i < samples - 1; i++) {
    for (int j = i + 1; j < samples; j++) {
      if (raw[i] > raw[j]) { int t = raw[i]; raw[i] = raw[j]; raw[j] = t; }
    }
  }
  long sum = 0;
  for (int i = 25; i < 75; i++) sum += raw[i];
  float avgRaw = sum / 50.0f;
  float voltage = avgRaw * (3.3f / 4095.0f);
  return abs((voltage - vref) / sensitivity);
}