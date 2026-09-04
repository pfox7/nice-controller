#include "web_server.h"
#include "settings.h"
#include "wifi_manager.h"
#include "motor_control.h"
#include "current_sensor.h"
#include "ble_handler.h"
#include "logger.h"
#include "utils.h"
#include <LittleFS.h>
#include <Update.h>

WebServer server(80);
const char* HTML_FILE = "/index.html";

static unsigned long lastTestCommand = 0;
const unsigned long TEST_DEBOUNCE = 1000;

void initWebServer() {
  
  server.on("/", []() {
    if (LittleFS.exists(HTML_FILE)) {
      File file = LittleFS.open(HTML_FILE, "r");
      if (file && file.size() > 100) {
        server.streamFile(file, "text/html");
        file.close();
        return;
      }
      if (file) file.close();
    }
    server.send(200, "text/html", getMinimalHTML());
  });
  
  server.on("/favicon.ico", []() { server.send(204); });
  server.on("/generate_204", []() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
  });
  
  server.on("/scan", []() { server.send(200, "application/json", scanWiFiNetworks()); });
  
  server.on("/connect", HTTP_POST, []() {
    String ssid = server.arg("ssid").substring(0, sizeof(settings.sta_ssid)-1);
    String pass = server.arg("password").substring(0, sizeof(settings.sta_password)-1);
    if (ssid.length() == 0) { server.send(400, "text/plain", "SSID required"); return; }
    strncpy(settings.sta_ssid, ssid.c_str(), sizeof(settings.sta_ssid)-1);
    settings.sta_ssid[sizeof(settings.sta_ssid)-1] = '\0';
    strncpy(settings.sta_password, pass.c_str(), sizeof(settings.sta_password)-1);
    settings.sta_password[sizeof(settings.sta_password)-1] = '\0';
    saveSettings();
    startWiFiConnect(ssid.c_str(), pass.c_str());
    server.send(200, "application/json", "{\"status\":\"started\"}");
  });
  
  server.on("/connect_status", []() { server.send(200, "application/json", getWiFiConnectStatus()); });
  
  server.on("/open",  []() { startForward(); server.send(200, "text/plain", "ОТКРЫТЬ"); });
  server.on("/close", []() { startReverse(); server.send(200, "text/plain", "ЗАКРЫТЬ"); });
  server.on("/stop",  []() { stopMotor();    server.send(200, "text/plain", "СТОП"); });
  
  server.on("/status", []() {
    float current = readCurrent();
    String json = "{\"state\":\"" + getStatusString() + "\",\"current\":" + String(current) + "}";
    server.send(200, "application/json", json);
  });
  
  server.on("/info", []() {
    String json = "{";
    json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    json += "\"ap_ip\":\"" + WiFi.softAPIP().toString() + "\",";
    json += "\"ssid\":\"" + String(apMode ? getApSSID() : settings.sta_ssid) + "\",";
    json += "\"mode\":\"" + String(apMode ? "AP" : "STA") + "\",";
    json += "\"hostname\":\"" + String(settings.device_name) + ".local\",";
    json += "\"device\":\"" + String(settings.device_name) + "\",";
    json += "\"mqtt_server\":\"" + String(settings.mqtt_server) + "\",";
    json += "\"mqtt_port\":" + String(settings.mqtt_port) + ",";
    json += "\"mqtt_user\":\"" + String(settings.mqtt_user) + "\",";
    json += "\"mqtt_topic\":\"" + String(settings.mqtt_topic) + "\",";
    json += "\"mqtt_retain\":" + String(settings.mqtt_retain ? "true" : "false") + ",";
    json += "\"ble_enabled\":" + String(settings.ble_enabled ? "true" : "false") + ",";
    json += "\"ble_mode\":\"" + String(settings.ble_hid_mode ? "HID" : "UART") + "\",";
    json += "\"version\":\"" + String(FIRMWARE_VERSION) + "\",";
    json += "\"tls_available\":false";
    json += "}";
    server.send(200, "application/json", json);
  });
  
  server.on("/mqtt_save", HTTP_POST, []() {
    if (server.hasArg("mqtt_server")) server.arg("mqtt_server").toCharArray(settings.mqtt_server, 65);
    if (server.hasArg("mqtt_port")) settings.mqtt_port = server.arg("mqtt_port").toInt();
    if (server.hasArg("mqtt_user")) server.arg("mqtt_user").toCharArray(settings.mqtt_user, 33);
    if (server.hasArg("mqtt_password")) server.arg("mqtt_password").toCharArray(settings.mqtt_password, 33);
    if (server.hasArg("mqtt_topic")) server.arg("mqtt_topic").toCharArray(settings.mqtt_topic, 65);
    settings.mqtt_retain = (server.arg("mqtt_retain") == "1");
    saveSettings();
    server.send(200, "text/plain", "MQTT сохранён");
  });
  
  server.on("/ble_save", HTTP_POST, []() {
    settings.ble_enabled = (server.arg("ble_enabled") == "1");
    settings.ble_hid_mode = (server.arg("ble_mode") == "1");
    saveSettings();
    if (settings.ble_enabled) { stopBLE(); initBLE(); } else stopBLE();
    server.send(200, "text/plain", "BLE сохранён");
  });
  
  server.on("/bonded_devices", []() {
    String json = "[";
    bool first = true;
    for (int i = 0; i < MAX_BONDED_DEVICES; i++) {
      if (bondedDevices[i].active) {
        if (!first) json += ",";
        json += "{\"address\":\"" + String(bondedDevices[i].address) + "\",";
        json += "\"name\":\"" + String(bondedDevices[i].name) + "\",";
        json += "\"pin\":\"" + String(bondedDevices[i].pin) + "\"}";
        first = false;
      }
    }
    json += "]";
    server.send(200, "application/json", json);
  });
  
  server.on("/remove_device", []() {
    if (server.hasArg("address")) {
      if (removeBondedDevice(server.arg("address"))) server.send(200, "text/plain", "Удалено");
      else server.send(404, "text/plain", "Не найдено");
    } else server.send(400, "text/plain", "Некорректный запрос");
  });
  
  // OTA-обновление
  server.on("/update", HTTP_POST, []() {
    server.send(200, "text/plain", Update.hasError() ? "ОШИБКА" : "OK. Перезагрузка...");
    delay(1000); ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Update.begin(UPDATE_SIZE_UNKNOWN);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      Update.write(upload.buf, upload.currentSize);
    } else if (upload.status == UPLOAD_FILE_END) {
      Update.end(true);
    }
  });
  
  server.on("/reset", []() {
    resetSettings(); saveSettings();
    server.send(200, "text/plain", "Настройки сброшены. Перезагрузка...");
    delay(1000); ESP.restart();
  });
  
  server.on("/reboot", []() {
    server.send(200, "text/plain", "Перезагрузка...");
    delay(1000); ESP.restart();
  });
  
  server.on("/clearlog", []() { logClear(); server.send(200, "text/plain", "Лог очищен"); });
  
  server.on("/setloglevel", HTTP_POST, []() {
    if (server.hasArg("level")) {
      int lvl = server.arg("level").toInt();
      if (lvl >= 0 && lvl <= 3) {
        setLogLevel((LogLevel)lvl);
        server.send(200, "text/plain", "Уровень установлен");
      } else {
        server.send(400, "text/plain", "Неверный уровень");
      }
    } else {
      server.send(400, "text/plain", "Нет параметра level");
    }
  });
  
  server.on("/getloglevel", []() { server.send(200, "text/plain", String(getLogLevel())); });
  
  // Тестовые команды
  server.on("/testopen",  []() { testForward(); server.send(200, "text/plain", "Тест ОТКРЫТЬ"); });
  server.on("/testclose", []() { testReverse(); server.send(200, "text/plain", "Тест ЗАКРЫТЬ"); });
  server.on("/teststop",  []() { testStop();    server.send(200, "text/plain", "Тест СТОП"); });
  
  server.on("/testfca", []() { server.send(200, "text/plain", readFCA() ? "разомкнут" : "замкнут"); });
  server.on("/testfcc", []() { server.send(200, "text/plain", readFCC() ? "разомкнут" : "замкнут"); });
  
  server.on("/testwaitfca", []() {
    server.sendHeader("Content-Type", "text/html; charset=utf-8");
    server.send(200, "text/html", R"rawliteral(
      <html><body style="background:#1a1a1a;color:#fff;font-family:Arial;text-align:center;padding:20px">
      <h2>Замкните концевик FCA (Открыто)</h2>
      <p>Соедините GND и D7 перемычкой, затем нажмите "Проверить"</p>
      <button onclick="check()">Проверить</button>
      <script>
        function check(){
          fetch('/testfca').then(r=>r.text()).then(t=>{
            if(t=='замкнут') alert('Концевик FCA замкнут!');
            else alert('Концевик всё ещё разомкнут');
          });
        }
      </script>
      </body></html>
    )rawliteral");
  });
  
  server.on("/testwaitfcc", []() {
    server.sendHeader("Content-Type", "text/html; charset=utf-8");
    server.send(200, "text/html", R"rawliteral(
      <html><body style="background:#1a1a1a;color:#fff;font-family:Arial;text-align:center;padding:20px">
      <h2>Замкните концевик FCC (Закрыто)</h2>
      <p>Соедините GND и D6 перемычкой, затем нажмите "Проверить"</p>
      <button onclick="check()">Проверить</button>
      <script>
        function check(){
          fetch('/testfcc').then(r=>r.text()).then(t=>{
            if(t=='замкнут') alert('Концевик FCC замкнут!');
            else alert('Концевик всё ещё разомкнут');
          });
        }
      </script>
      </body></html>
    )rawliteral");
  });
  
  server.on("/uploadhtml", HTTP_POST, []() {
    server.send(200, "text/plain", "OK. Перезагрузите страницу.");
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      File f = LittleFS.open("/index.html", "w");
      if (!f) return;
      f.close();
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      File f = LittleFS.open("/index.html", "a");
      if (f) {
        f.write((uint8_t*)upload.buf, upload.currentSize);
        f.close();
      }
    }
  });
  
  server.onNotFound([]() { server.send(404, "text/plain", "Not Found"); });
  
  server.on("/log", []() { server.send(200, "text/plain; charset=utf-8", logRead()); });
  
  server.begin();
  logMessage("Веб-сервер: запущен на порту 80");
}

void handleWebServer() { server.handleClient(); }

String getMinimalHTML() {
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Gate Controller</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,sans-serif;background:#1a1a1a;color:#fff;padding:10px}
h2{text-align:center;margin:10px 0}
button{font-size:24px;padding:20px;margin:10px;border-radius:12px;border:none;width:90%;cursor:pointer;color:#fff}
.open{background:#4CAF50}.stop{background:#f44336}.close{background:#FF9800}
.status{text-align:center;margin-top:20px;font-size:16px;background:#333;padding:10px;border-radius:8px}
.info{background:#333;padding:10px;border-radius:8px;text-align:center;margin:10px 0}
.wifi-btn{background:#2196F3;margin-top:10px}
</style>
</head>
<body>
<h2>Управление воротами<br><small id="ver">...</small></h2>
<div class="info">
  IP: <span id="ip">--</span> | Имя: <span id="host">gatecontroller.local</span>
</div>
<button class="open" onclick="fetch('/open')">ОТКРЫТЬ</button>
<button class="stop" onclick="fetch('/stop')">СТОП</button>
<button class="close" onclick="fetch('/close')">ЗАКРЫТЬ</button>
<button class="wifi-btn" onclick="openWifiModal()">Wi-Fi</button>
<button class="wifi-btn" onclick="document.getElementById('htmlFileInput').click()">Обновить HTML</button>
<input type="file" id="htmlFileInput" accept=".html" style="display:none" onchange="uploadHTML(this.files[0])">
<div class="status">
  Состояние: <span id="st">-</span> | Ток: <span id="cur">0.00</span> А
</div>

<div id="wifiModal" style="display:none;position:fixed;z-index:100;left:0;top:0;width:100%;height:100%;background:rgba(0,0,0,0.7)">
  <div style="background:#222;margin:15% auto;padding:20px;border-radius:10px;width:90%;max-width:400px;text-align:center">
    <h3>Подключение к Wi-Fi</h3>
    <div id="scanStatus"></div>
    <input type="text" id="ssidInput" placeholder="SSID сети" list="ssidList" style="width:100%;padding:10px;margin:5px 0;border-radius:8px;border:none;font-size:16px;background:#333;color:#fff">
    <datalist id="ssidList"></datalist>
    <input type="text" id="passInput" placeholder="Пароль" style="width:100%;padding:10px;margin:5px 0;border-radius:8px;border:none;font-size:16px;background:#333;color:#fff">
    <button onclick="startConnect()" style="background:#4CAF50;color:#fff;padding:10px 30px;border:none;border-radius:8px;font-size:16px;cursor:pointer;margin:5px">Подключиться</button>
    <button onclick="closeWifiModal()" style="background:#f44336;color:#fff;padding:10px 30px;border:none;border-radius:8px;font-size:16px;cursor:pointer;margin:5px">Отмена</button>
    <div id="connectStatus"></div>
  </div>
</div>

<script>
function updateInfo(){
  fetch('/info').then(r=>r.json()).then(d=>{
    document.getElementById('ip').innerText = d.ip||'--';
    document.getElementById('host').innerText = d.hostname||'gatecontroller.local';
    document.getElementById('ver').innerText = d.version||'';
  });
}
function updateStatus(){
  fetch('/status').then(r=>r.json()).then(d=>{
    document.getElementById('st').innerText = d.state||'-';
    document.getElementById('cur').innerText = d.current?d.current.toFixed(2):'0.00';
  });
}
setInterval(updateStatus,2000);
setInterval(updateInfo,5000);
updateStatus();updateInfo();

function openWifiModal(){
  document.getElementById('wifiModal').style.display='block';
  document.getElementById('scanStatus').innerHTML='Загрузка...';
  fetch('/scan').then(r=>r.json()).then(nets=>{
    let datalist = document.getElementById('ssidList');
    datalist.innerHTML = '';
    nets.forEach(n=>{
      let opt = document.createElement('option');
      opt.value = n.ssid;
      datalist.appendChild(opt);
    });
    document.getElementById('scanStatus').innerHTML='Готово';
  });
}
function closeWifiModal(){ document.getElementById('wifiModal').style.display='none'; }

async function startConnect(){
  let ssid = document.getElementById('ssidInput').value.trim();
  let pass = document.getElementById('passInput').value;
  if(!ssid) return;
  let btn = event.target;
  btn.disabled = true;
  btn.textContent = 'Подключение...';
  let formData = new FormData();
  formData.append('ssid', ssid);
  formData.append('password', pass);
  let resp = await fetch('/connect', {method:'POST', body: formData});
  let result = await resp.json();
  if(result.status == 'started'){
    let interval = setInterval(async ()=>{
      let s = await fetch('/connect_status');
      let status = await s.json();
      if(status.status == 'connected'){
        clearInterval(interval);
        document.getElementById('connectStatus').innerHTML = '<p style="color:#4CAF50">Подключено! IP: '+status.ip+'</p>';
        setTimeout(()=>{ location.href = 'http://'+status.ip+'/'; }, 3000);
      } else if(status.status == 'failed'){
        clearInterval(interval);
        document.getElementById('connectStatus').innerHTML = '<p style="color:#f44336">Ошибка подключения</p>';
        btn.disabled = false;
        btn.textContent = 'Подключиться';
      }
    }, 1000);
  } else {
    document.getElementById('connectStatus').innerHTML = '<p style="color:#f44336">Ошибка запуска</p>';
    btn.disabled = false;
    btn.textContent = 'Подключиться';
  }
}

function uploadHTML(file) {
  if (!file) return;
  var formData = new FormData();
  formData.append('file', file);
  fetch('/uploadhtml', {method:'POST', body: formData})
    .then(r => r.text())
    .then(t => {
      alert('HTML обновлён. Перезагружаем...');
      setTimeout(() => location.reload(), 1000);
    });
}
</script>
</body>
</html>
)rawliteral";
}