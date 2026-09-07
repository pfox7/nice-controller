#include "motor_control.h"
#include "utils.h"
#include "current_sensor.h"

State currentState = IDLE;
unsigned long moveStartTime = 0;
bool testMode = false;

static bool lastDirectionForward = true; // направление последнего движения (true = открытие)

// Защита от слишком быстрого переключения
static unsigned long lastDirectionChange = 0;
const unsigned long MIN_DIRECTION_CHANGE_INTERVAL = 800; // мс

// Блокировка запуска сразу после остановки
static unsigned long lastStopTime = 0;
const unsigned long MIN_STOP_INTERVAL = 300; // мс

void motorSetup() {
  // Безопасная инициализация пинов
  digitalWrite(SSR_MAIN_PIN, SSR_OFF);
  digitalWrite(RELAY_CAP_PIN, RELAY_OFF);
  digitalWrite(START_RELAY_PIN, START_SSR_OFF);   // пусковой SSR выключен
  digitalWrite(LED_PIN, LOW);
  
  pinMode(SSR_MAIN_PIN, OUTPUT);
  pinMode(RELAY_CAP_PIN, OUTPUT);
  pinMode(START_RELAY_PIN, OUTPUT);
  pinMode(FCA_PIN, INPUT_PULLUP);
  pinMode(FCC_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  // Чтобы первый запуск не блокировался
  lastStopTime = millis() - 1000;
  lastDirectionChange = millis() - 1000;

  logMessage("Двигатель: инициализирован (1 SSR + 2 реле)");
}

void startForward() {
  testMode = false;
  
  if (millis() - lastStopTime < MIN_STOP_INTERVAL) {
    logMessage("Двигатель: слишком рано после остановки, ОТКРЫТЬ игнорируется");
    return;
  }
  
  if (millis() - lastDirectionChange < MIN_DIRECTION_CHANGE_INTERVAL) {
    logMessage("Двигатель: слишком частое переключение, ОТКРЫТЬ игнорируется");
    return;
  }
  
  if (digitalRead(FCA_PIN) == HIGH && currentState == IDLE) {
    logMessage("Двигатель: уже открыто, ОТКРЫТЬ заблокирован");
    return;
  }
  
  stopMotor();
  delay(100);                               // ждём полного выключения SSR
  digitalWrite(RELAY_CAP_PIN, RELAY_OFF);   // сначала переключаем реле (без тока)
  delay(20);                                // даём контактам успокоиться
  digitalWrite(SSR_MAIN_PIN, SSR_ON);       // затем включаем SSR
  
  currentState = MOVING_FORWARD;
  moveStartTime = millis();
  digitalWrite(LED_PIN, HIGH);
  lastDirectionForward = true;
  lastDirectionChange = millis();
  logMessage("Двигатель: ОТКРЫТЬ");
}

void startReverse() {
  testMode = false;
  
  if (millis() - lastStopTime < MIN_STOP_INTERVAL) {
    logMessage("Двигатель: слишком рано после остановки, ЗАКРЫТЬ игнорируется");
    return;
  }
  
  if (millis() - lastDirectionChange < MIN_DIRECTION_CHANGE_INTERVAL) {
    logMessage("Двигатель: слишком частое переключение, ЗАКРЫТЬ игнорируется");
    return;
  }
  
  if (digitalRead(FCC_PIN) == HIGH && currentState == IDLE) {
    logMessage("Двигатель: уже закрыто, ЗАКРЫТЬ заблокирован");
    return;
  }
  
  stopMotor();
  delay(100);
  digitalWrite(RELAY_CAP_PIN, RELAY_ON);    // сначала реле
  delay(20);
  digitalWrite(SSR_MAIN_PIN, SSR_ON);       // затем SSR
  
  currentState = MOVING_REVERSE;
  moveStartTime = millis();
  digitalWrite(LED_PIN, HIGH);
  lastDirectionForward = false;
  lastDirectionChange = millis();
  logMessage("Двигатель: ЗАКРЫТЬ");
}

void stopMotor() {
  testMode = false;
  
  digitalWrite(SSR_MAIN_PIN, SSR_OFF);      // сначала выключаем SSR
  delay(20);                                // ждём размыкания симистора
  digitalWrite(RELAY_CAP_PIN, RELAY_OFF);
  digitalWrite(START_RELAY_PIN, START_SSR_OFF);   // выключаем пусковой SSR
  
  currentState = IDLE;
  digitalWrite(LED_PIN, LOW);
  
  lastStopTime = millis();
  
  logMessage("Двигатель: СТОП");
}

String getStatusString() {
  switch (currentState) {
    case IDLE:              return "СТОП";
    case MOVING_FORWARD:    return "ОТКРЫТЬ";
    case MOVING_REVERSE:    return "ЗАКРЫТЬ";
    case OBSTACLE_BACKWARD: return "ОТКАТ";
    default:                return "НЕИЗВЕСТНО";
  }
}

String getPositionString() {
  bool fca = digitalRead(FCA_PIN);
  bool fcc = digitalRead(FCC_PIN);
  if (fca == HIGH && fcc == LOW) return "OPEN";
  if (fca == LOW && fcc == HIGH) return "CLOSED";
  return "MIDDLE";
}

void handleObstacle(String direction) {
  logMessage("Двигатель: обнаружено препятствие при движении " + direction);
  stopMotor();
  delay(300);
  
  if (direction == "FORWARD") {
    digitalWrite(RELAY_CAP_PIN, RELAY_ON);
    delay(20);
    digitalWrite(SSR_MAIN_PIN, SSR_ON);
    currentState = MOVING_REVERSE;
    lastDirectionForward = false;
  } else {
    digitalWrite(RELAY_CAP_PIN, RELAY_OFF);
    delay(20);
    digitalWrite(SSR_MAIN_PIN, SSR_ON);
    currentState = MOVING_FORWARD;
    lastDirectionForward = true;
  }
  
  moveStartTime = millis();
  digitalWrite(LED_PIN, HIGH);
  currentState = OBSTACLE_BACKWARD;
  
  lastDirectionChange = millis();
  lastStopTime = millis() - MIN_STOP_INTERVAL;
  
  logMessage("Двигатель: откат запущен");
}

void testForward() {
  testMode = true;
  stopMotor();
  delay(100);
  digitalWrite(RELAY_CAP_PIN, RELAY_OFF);
  delay(20);
  digitalWrite(SSR_MAIN_PIN, SSR_ON);
  currentState = MOVING_FORWARD;
  moveStartTime = millis();
  digitalWrite(LED_PIN, HIGH);
  lastDirectionForward = true;
  logMessage("Тест: ОТКРЫТЬ (концевики игнорируются)");
}

void testReverse() {
  testMode = true;
  stopMotor();
  delay(100);
  digitalWrite(RELAY_CAP_PIN, RELAY_ON);
  delay(20);
  digitalWrite(SSR_MAIN_PIN, SSR_ON);
  currentState = MOVING_REVERSE;
  moveStartTime = millis();
  digitalWrite(LED_PIN, HIGH);
  lastDirectionForward = false;
  logMessage("Тест: ЗАКРЫТЬ (концевики игнорируются)");
}

void testStop() {
  testMode = false;
  stopMotor();
  logMessage("Тест: СТОП");
}

bool readFCA() { return digitalRead(FCA_PIN); }
bool readFCC() { return digitalRead(FCC_PIN); }

void handleStartRelay() {
  static bool startRelayActive = false;
  static unsigned long startTime = 0;
  static bool startDone = false;

  if (currentState == MOVING_FORWARD || currentState == MOVING_REVERSE) {
    if (!startRelayActive && !startDone) {
      digitalWrite(START_RELAY_PIN, START_SSR_ON);   // включаем пусковой SSR
      startRelayActive = true;
      startTime = millis();
      logMessage("Пусковой конденсатор: ВКЛ");
    } else if (startRelayActive) {
      float current = readCurrent();
      if ((current < START_CURRENT_THRESH) && (millis() - startTime > 1000)) {
        digitalWrite(START_RELAY_PIN, START_SSR_OFF);  // выключаем
        startRelayActive = false;
        startDone = true;
        logMessage("Пусковой конденсатор: ОТКЛ (ток упал)");
      } else if (millis() - startTime > START_MAX_TIME) {
        digitalWrite(START_RELAY_PIN, START_SSR_OFF);
        startRelayActive = false;
        startDone = true;
        logMessage("Пусковой конденсатор: ОТКЛ (таймаут)");
      }
    }
  } else {
    digitalWrite(START_RELAY_PIN, START_SSR_OFF);
    startRelayActive = false;
    startDone = false;
  }
}

// ========== ОБРАБОТКА КНОПКИ ==========
void handleButton() {
  static unsigned long lastDebounceTime = 0;
  static int lastButtonState = HIGH;
  static int buttonState = HIGH;

  int reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > 100) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == BUTTON_ACTIVE_STATE) {
        logMessage("Кнопка нажата");

        if (currentState == MOVING_FORWARD || currentState == MOVING_REVERSE || currentState == OBSTACLE_BACKWARD) {
          stopMotor();
        } else {
          bool fca = digitalRead(FCA_PIN);
          bool fcc = digitalRead(FCC_PIN);

          if (lastDirectionForward) {
            if (!fcc) {
              startReverse();
            } else {
              startForward();
            }
          } else {
            if (!fca) {
              startForward();
            } else {
              startReverse();
            }
          }
        }
      }
    }
  }

  lastButtonState = reading;
}