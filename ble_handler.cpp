#include "ble_handler.h"
#include "motor_control.h"

BLEServer* pServer = nullptr;
BLECharacteristic* pTxCharacteristic = nullptr;
BLECharacteristic* pAuthCharacteristic = nullptr;
bool deviceAuthenticated[MAX_BONDED_DEVICES] = {false};
String pendingAddress = "";
bool bleDeviceConnected = false;

class AuthCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    String value = pCharacteristic->getValue().c_str();
    
    if (value.startsWith("AUTH:")) {
      String pin = value.substring(5);
      int idx = findBondedDevice(pendingAddress);
      
      if (idx >= 0 && String(bondedDevices[idx].pin) == pin) {
        deviceAuthenticated[idx] = true;
        pAuthCharacteristic->setValue("OK");
        Serial.println("BLE: устройство авторизовано: " + pendingAddress);
      } else {
        pAuthCharacteristic->setValue("DENIED");
        Serial.println("BLE: авторизация не удалась: " + pendingAddress);
      }
      pAuthCharacteristic->notify();
    }
  }
};

class CommandCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    String command = pCharacteristic->getValue().c_str();
    command.trim();
    
    Serial.println("BLE: получена команда: " + command);
    
    if (command == "FORWARD" || command == "F") {
      startForward();
    } else if (command == "REVERSE" || command == "R") {
      startReverse();
    } else if (command == "STOP" || command == "S") {
      stopMotor();
    } else if (command == "STATUS" || command == "?") {
      if (pTxCharacteristic != nullptr) {
        pTxCharacteristic->setValue(getStatusString().c_str());
        pTxCharacteristic->notify();
      }
    }
  }
};

class SecureServerCallback : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer, esp_ble_gatts_cb_param_t* param) {
    char addr[18];
    snprintf(addr, sizeof(addr), "%02X:%02X:%02X:%02X:%02X:%02X",
             param->connect.remote_bda[0], param->connect.remote_bda[1],
             param->connect.remote_bda[2], param->connect.remote_bda[3],
             param->connect.remote_bda[4], param->connect.remote_bda[5]);
    
    pendingAddress = String(addr);
    Serial.println("BLE: подключено устройство: " + pendingAddress);
    bleDeviceConnected = true;
    
    int idx = findBondedDevice(pendingAddress);
    if (idx >= 0) {
      deviceAuthenticated[idx] = true;
      Serial.println("BLE: известное устройство, авторизовано автоматически");
    } else {
      String newPin = generatePin();
      int newIdx = addBondedDevice(pendingAddress, "iPhone", newPin);
      if (newIdx >= 0 && pAuthCharacteristic != nullptr) {
        String pinMsg = "PIN:" + newPin;
        pAuthCharacteristic->setValue(pinMsg.c_str());
        pAuthCharacteristic->notify();
        Serial.println("BLE: новое устройство, отправлен ПИН: " + newPin);
      }
    }
  }
  
  void onDisconnect(BLEServer* pServer) {
    Serial.println("BLE: устройство отключено: " + pendingAddress);
    bleDeviceConnected = false;
    int idx = findBondedDevice(pendingAddress);
    if (idx >= 0) {
      deviceAuthenticated[idx] = false;
    }
  }
};

void initBLE() {
  if (!settings.ble_enabled) {
    Serial.println("BLE: отключен в настройках");
    return;
  }
  
  Serial.print("BLE: инициализация как ");
  Serial.println(settings.device_name);
  
  BLEDevice::init(settings.device_name);
  
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new SecureServerCallback());
  
  BLEService* pService = pServer->createService(BLE_SERVICE_UUID);
  
  pAuthCharacteristic = pService->createCharacteristic(
    BLE_CHARACTERISTIC_UUID_AUTH,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pAuthCharacteristic->setCallbacks(new AuthCallback());
  pAuthCharacteristic->addDescriptor(new BLE2902());
  
  BLECharacteristic* pCommandCharacteristic = pService->createCharacteristic(
    BLE_CHARACTERISTIC_UUID_RX,
    BLECharacteristic::PROPERTY_WRITE
  );
  pCommandCharacteristic->setCallbacks(new CommandCallback());
  
  pTxCharacteristic = pService->createCharacteristic(
    BLE_CHARACTERISTIC_UUID_TX,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pTxCharacteristic->addDescriptor(new BLE2902());
  
  pService->start();
  
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  BLEDevice::startAdvertising();
  
  Serial.println("BLE: запущен, ожидание подключений...");
}

void stopBLE() {
  if (pServer != nullptr) {
    BLEDevice::deinit(true);
    pServer = nullptr;
    pTxCharacteristic = nullptr;
    pAuthCharacteristic = nullptr;
    Serial.println("BLE: остановлен");
  }
}

void handleBLE() {
  // Обработка в коллбэках
}