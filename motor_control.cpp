#include "motor_control.h"
#include "utils.h"
#include "current_sensor.h"

State currentState = IDLE;
unsigned long moveStartTime = 0;
bool testMode = false;

static bool lastDirectionForward = true; // направление последнего движения (true = открытие)

// Минимальный интервал между сменой направления (для защиты реле)
static unsigned long lastDirectionChange = 0;
const unsigned long MIN_DIRECTION_CHANGE_INTERVAL = 500; // мс

void motorSetup() {
  // Сначала устанавливаем безопасные уровни, потом режимы OUTPUT
  digitalWrite(SSR_MAIN_PIN, SSR_OFF);
  digitalWrite(RELAY_CAP_PIN, RELAY_OFF);
  digitalWrite(START_RELAY_PIN, RELAY_OFF);
  digitalWrite(LED_PIN, LOW);
  
  pinMode(SSR_MAIN_PIN, OUTPUT);
  pinMode(RELAY_CAP_PIN, OUTPUT);
  pinMode(START_RELAY_PIN, OUTPUT);
  pinMode(FCA_PIN, INPUT_PULLUP);
  pinMode(FCC_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  logMessage("Двигатель: инициализирован (1 SSR + 2 реле)");
}

void startForward() {
  testMode = false;
  
  // Защита от слишком быстрого переключения
  if (millis() - lastDirectionChange < MIN_DIRECTION_CHANGE_INTERVAL) {
    logMessage("Двигатель: слишком частое переключение, игнорируем ОТКРЫТЬ");
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
  lastDirectionForward = true;              // запоминаем направление
  lastDirectionChange = millis();           // фиксируем время
  logMessage("Двигатель: ОТКРЫТЬ");
}

void startReverse() {
  testMode = false;
  
  // Защита от слишком быстрого переключения
  if (millis() - lastDirectionChange < MIN_DIRECTION_CHANGE_INTERVAL) {
    logMessage("Двигатель: слишком частое переключение, игнорируем ЗАКРЫТЬ");
    return;
  }
  
  if (digitalRead(FCC_PIN) == HIGH && currentState == IDLE) {
    logMessage("Двигатель: уже закрыто, ЗАКРЫТЬ заблокирован");
    return;
  }
  
  stopMotor();
  delay(100);                               // ждём полного выключения SSR
  digitalWrite(RELAY_CAP_PIN, RELAY_ON);    // сначала переключаем реле (без тока)
  delay(20);                                // даём контактам успокоиться
  digitalWrite(SSR_MAIN_PIN, SSR_ON);       // затем включаем SSR
  
  currentState = MOVING_REVERSE;
  moveStartTime = millis();
  digitalWrite(LED_PIN, HIGH);
  lastDirectionForward = false;             // запоминаем направление
  lastDirectionChange = millis();           // фиксируем время
  logMessage("Двигатель: ЗАКРЫТЬ");
}

void stopMotor() {
  testMode = false;
  
  digitalWrite(SSR_MAIN_PIN, SSR_OFF);      // сначала выключаем SSR
  delay(20);                                // ждём размыкания симистора
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
  delay(300);  // пауза перед реверсом
  
  // Определяем новое направление и включаем с правильной последовательностью
  if (direction == "FORWARD") {
    // Было движение вперёд, теперь назад (закрытие)
    digitalWrite(RELAY_CAP_PIN, RELAY_ON);   // реле для закрытия
    delay(20);
    digitalWrite(SSR_MAIN_PIN, SSR_ON);
    currentState = MOVING_REVERSE;
    lastDirectionForward = false;
  } else {
    // Было движение назад, теперь вперёд (открытие)
    digitalWrite(RELAY_CAP_PIN, RELAY_OFF);  // реле для открытия
    delay(20);
    digitalWrite(SSR_MAIN_PIN, SSR_ON);
    currentState = MOVING_FORWARD;
    lastDirectionForward = true;
  }
  
  moveStartTime = millis();
  digitalWrite(LED_PIN, HIGH);
  currentState = OBSTACLE_BACKWARD;  // переопределяем состояние для логики отката
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

// ========== ОБРАБОТКА КНОПКИ (если используется) ==========
void handleButton() {
  static unsigned long lastDebounceTime = 0;
  static int lastButtonState = HIGH;      // начальное состояние (не нажата)
  static int buttonState = HIGH;          // стабильное состояние после дребезга

  int reading = digitalRead(BUTTON_PIN);

  // Если состояние изменилось, сбросить таймер дребезга
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  // Если состояние стабильно дольше 50 мс
  if ((millis() - lastDebounceTime) > 50) {
    // Если состояние изменилось с прошлого стабильного
    if (reading != buttonState) {
      buttonState = reading;
      // Реагируем только на нажатие (переход в активное состояние)
      if (buttonState == BUTTON_ACTIVE_STATE) {
        logMessage("Кнопка нажата");

        // Если двигатель движется — остановить
        if (currentState == MOVING_FORWARD || currentState == MOVING_REVERSE || currentState == OBSTACLE_BACKWARD) {
          stopMotor();
        } else {
          // Остановлен, решаем направление
          bool fca = digitalRead(FCA_PIN); // HIGH = нажат (открыто)
          bool fcc = digitalRead(FCC_PIN); // HIGH = нажат (закрыто)

          if (lastDirectionForward) {
            // Последнее движение было "открыть"
            if (!fcc) {
              // Не закрыто -> закрываем
              startReverse();
            } else {
              // Закрыто -> открываем
              startForward();
            }
          } else {
            // Последнее движение было "закрыть"
            if (!fca) {
              // Не открыто -> открываем
              startForward();
            } else {
              // Открыто -> закрываем
              startReverse();
            }
          }
        }
      }
    }
  }

  // Сохраняем текущее чтение для следующего цикла
  lastButtonState = reading;
}