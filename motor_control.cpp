#include "motor_control.h"
#include "utils.h"
#include "current_sensor.h"

State currentState = IDLE;
unsigned long moveStartTime = 0;
bool testMode = false;

void motorSetup() {
  pinMode(SSR_MAIN_PIN, OUTPUT);
  pinMode(RELAY_CAP_PIN, OUTPUT);
  pinMode(START_RELAY_PIN, OUTPUT);
  pinMode(FCA_PIN, INPUT_PULLUP);
  pinMode(FCC_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  digitalWrite(SSR_MAIN_PIN, SSR_OFF);
  digitalWrite(RELAY_CAP_PIN, RELAY_OFF);
  digitalWrite(START_RELAY_PIN, RELAY_OFF);
  digitalWrite(LED_PIN, LOW);
  logMessage("Двигатель: инициализирован (1 SSR + 2 реле)");
}

void startForward() {
  testMode = false;
  if (digitalRead(FCA_PIN) == HIGH && currentState == IDLE) {
    logMessage("Двигатель: уже открыто, ОТКРЫТЬ заблокирован");
    return;
  }
  stopMotor();
  delay(50);
  digitalWrite(SSR_MAIN_PIN, SSR_ON);
  digitalWrite(RELAY_CAP_PIN, RELAY_OFF);   // ИЗМЕНЕНО: конденсатор к B (для открытия)
  currentState = MOVING_FORWARD;
  moveStartTime = millis();
  digitalWrite(LED_PIN, HIGH);
  logMessage("Двигатель: ОТКРЫТЬ");
}

void startReverse() {
  testMode = false;
  if (digitalRead(FCC_PIN) == HIGH && currentState == IDLE) {
    logMessage("Двигатель: уже закрыто, ЗАКРЫТЬ заблокирован");
    return;
  }
  stopMotor();
  delay(50);
  digitalWrite(SSR_MAIN_PIN, SSR_ON);
  digitalWrite(RELAY_CAP_PIN, RELAY_ON);    // ИЗМЕНЕНО: конденсатор к A (для закрытия)
  currentState = MOVING_REVERSE;
  moveStartTime = millis();
  digitalWrite(LED_PIN, HIGH);
  logMessage("Двигатель: ЗАКРЫТЬ");
}

void stopMotor() {
  testMode = false;
  digitalWrite(SSR_MAIN_PIN, SSR_OFF);
  digitalWrite(RELAY_CAP_PIN, RELAY_OFF);
  digitalWrite(START_RELAY_PIN, RELAY_OFF);
  currentState = IDLE;
  digitalWrite(LED_PIN, LOW);
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
  if (direction == "FORWARD") startReverse();
  else startForward();
  currentState = OBSTACLE_BACKWARD;
  moveStartTime = millis();
}

void testForward() {
  testMode = true;
  stopMotor();
  delay(50);
  digitalWrite(SSR_MAIN_PIN, SSR_ON);
  digitalWrite(RELAY_CAP_PIN, RELAY_OFF);
  currentState = MOVING_FORWARD;
  moveStartTime = millis();
  digitalWrite(LED_PIN, HIGH);
  logMessage("Тест: ОТКРЫТЬ (концевики игнорируются)");
}

void testReverse() {
  testMode = true;
  stopMotor();
  delay(50);
  digitalWrite(SSR_MAIN_PIN, SSR_ON);
  digitalWrite(RELAY_CAP_PIN, RELAY_ON);
  currentState = MOVING_REVERSE;
  moveStartTime = millis();
  digitalWrite(LED_PIN, HIGH);
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
      digitalWrite(START_RELAY_PIN, RELAY_ON);
      startRelayActive = true;
      startTime = millis();
      logMessage("Пусковой конденсатор: ВКЛ");
    } else if (startRelayActive) {
      float current = readCurrent();
      if ((current < START_CURRENT_THRESH) && (millis() - startTime > 1000)) {
        digitalWrite(START_RELAY_PIN, RELAY_OFF);
        startRelayActive = false;
        startDone = true;
        logMessage("Пусковой конденсатор: ОТКЛ (ток упал)");
      } else if (millis() - startTime > START_MAX_TIME) {
        digitalWrite(START_RELAY_PIN, RELAY_OFF);
        startRelayActive = false;
        startDone = true;
        logMessage("Пусковой конденсатор: ОТКЛ (таймаут)");
      }
    }
  } else {
    digitalWrite(START_RELAY_PIN, RELAY_OFF);
    startRelayActive = false;
    startDone = false;
  }
}