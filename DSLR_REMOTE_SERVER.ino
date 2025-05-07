#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>

// ========== НАСТРОЙКИ ==========
const char* ssid = "DSLR_Remote";       // Имя точки доступа
const char* password = "12345678";      // Пароль точки доступа

ESP8266WebServer server(80);

#define DEFAULT_PRESS_DELAY 100
#define DEFAULT_PRESS_DURATION 100
#define DEFAULT_INTERVAL 1000
#define DEFAULT_SHOT_COUNT 1
#define DEFAULT_SENSOR_TIMEOUT 5
#define SHUTTER_PIN D2
#define SENSOR1_PIN D6
#define SENSOR2_PIN D7
#define SENSOR1_INVERTED false
#define SENSOR2_INVERTED true
#define DEBOUNCE_DELAY 50
#define LED_BUILTINN D4
// ==============================



enum ShootingMode { MODE_BUTTON, MODE_SENSOR1, MODE_SENSOR2, MODE_INTERVAL };

// Настройки
ShootingMode currentMode = MODE_BUTTON;
unsigned long pressDelay = DEFAULT_PRESS_DELAY;
unsigned long pressDuration = DEFAULT_PRESS_DURATION;
unsigned long interval = DEFAULT_INTERVAL;
unsigned long shotCount = DEFAULT_SHOT_COUNT;
unsigned long sensorTimeout = DEFAULT_SENSOR_TIMEOUT * 1000;
bool sensor1Inverted = SENSOR1_INVERTED;
bool sensor2Inverted = SENSOR2_INVERTED;

// Состояние
unsigned long totalShots = 0;
bool isShooting = false;
unsigned long lastShotTime = 0;
unsigned long shotsTaken = 0;
unsigned long sensorActiveStart = 0;
bool sensorActive = false;
bool intervalRunning = false;
bool lastSensor1State = HIGH;
bool lastSensor2State = HIGH;
unsigned long lastDebounceTime1 = 0;
unsigned long lastDebounceTime2 = 0;

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><title>DSLR Remote Control</title>";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<style>body{font-family:Arial,sans-serif;margin:20px;max-width:800px;}";
  html += "button{padding:10px 15px;margin:5px;font-size:16px;border-radius:5px;}";
  html += "input{padding:8px;margin:5px;width:100px;}";
  html += ".manual-btn{background:#4CAF50;color:white;font-size:20px;padding:15px 30px;}";
  html += ".start-btn{background:#2196F3;color:white;font-size:20px;padding:15px 30px;}";
  html += ".stats{background:#f5f5f5;padding:15px;border-radius:5px;margin:20px 0;}";
  html += ".warning{color:red;font-weight:bold;}";
  html += "</style></head><body>";
  html += "<h1>DSLR Remote Control</h1>";
  
  // Статистика
  html += "<div class=\"stats\">";
  html += "<h2>Statistics</h2>";
  html += "Total shots: <strong>" + String(totalShots) + "</strong><br>";
  html += "Session shots: <strong>" + String(shotsTaken) + "</strong><br>";
  html += "<form action=\"/resetstats\" style=\"display:inline;\">";
  html += "<button type=\"submit\">Reset Stats</button>";
  html += "</form>";
  html += "</div>";
  
  // Выбор режима
  html += "<h2>Shooting Mode</h2>";
  html += "<form action=\"/setmode\">";
  html += "<button type=\"submit\" name=\"mode\" value=\"button\"" + String(currentMode == MODE_BUTTON ? " disabled" : "") + ">Phone Button</button>";
  html += "<button type=\"submit\" name=\"mode\" value=\"sensor1\"" + String(currentMode == MODE_SENSOR1 ? " disabled" : "") + ">Sensor 1 (D6)</button>";
  html += "<button type=\"submit\" name=\"mode\" value=\"sensor2\"" + String(currentMode == MODE_SENSOR2 ? " disabled" : "") + ">Sensor 2 (D7)</button>";
  html += "<button type=\"submit\" name=\"mode\" value=\"interval\"" + String(currentMode == MODE_INTERVAL ? " disabled" : "") + ">Interval Shooting</button>";
  html += "</form>";
  
  // Настройки
  html += "<h2>Settings</h2>";
  html += "<form action=\"/setsettings\">";
  html += "Press Delay (ms): <input type=\"number\" name=\"delay\" value=\"" + String(pressDelay) + "\"><br>";
  html += "Press Duration (ms): <input type=\"number\" name=\"duration\" value=\"" + String(pressDuration) + "\"><br>";
  
  if(currentMode == MODE_INTERVAL) {
    unsigned long minInterval = pressDelay + pressDuration;
    if(interval < minInterval) {
      html += "<p class=\"warning\">Interval must be ≥ " + String(minInterval) + "ms!</p>";
    }
    html += "Interval (ms): <input type=\"number\" name=\"interval\" value=\"" + String(interval) + "\" min=\"" + String(minInterval) + "\"><br>";
    html += "Shot Count (0=unlimited): <input type=\"number\" name=\"count\" value=\"" + String(shotCount) + "\"><br>";
  }
  
  if(currentMode == MODE_SENSOR1 || currentMode == MODE_SENSOR2) {
    unsigned long minTimeout = (pressDelay + pressDuration) / 1000 + 1;
    if(sensorTimeout/1000 < minTimeout) {
      html += "<p class=\"warning\">Timeout must be ≥ " + String(minTimeout) + "s!</p>";
    }
    html += "Sensor Timeout (sec): <input type=\"number\" name=\"timeout\" value=\"" + String(sensorTimeout/1000) + "\" min=\"" + String(minTimeout) + "\"><br>";
    
    if(currentMode == MODE_SENSOR1) {
      html += "Invert Sensor: <input type=\"checkbox\" name=\"invert\" value=\"1\"" + String(sensor1Inverted ? " checked" : "") + "><br>";
    } else if(currentMode == MODE_SENSOR2) {
      html += "Invert Sensor: <input type=\"checkbox\" name=\"invert\" value=\"1\"" + String(sensor2Inverted ? " checked" : "") + "><br>";
    }
  }
  
  html += "<button type=\"submit\">Save Settings</button>";
  html += "</form>";
  
  // Ручной спуск
  html += "<h2>Manual Trigger</h2>";
  html += "<button class=\"manual-btn\" onclick=\"triggerShutter()\">TRIGGER SHUTTER</button>";
  html += "<script>function triggerShutter(){fetch('/trigger');}</script>";
  
  if(currentMode == MODE_INTERVAL) {
    html += "<h2>Interval Shooting Control</h2>";
    if(!intervalRunning) {
      html += "<button class=\"start-btn\" onclick=\"startInterval()\">START INTERVAL</button>";
    } else {
      html += "<button class=\"start-btn\" onclick=\"stopInterval()\">STOP INTERVAL</button>";
      html += "<p>Progress: " + String(shotsTaken) + " of " + (shotCount == 0 ? "∞" : String(shotCount)) + " shots</p>";
    }
    html += "<script>";
    html += "function startInterval(){fetch('/startinterval'); setTimeout(function(){location.reload();},500);}";
    html += "function stopInterval(){fetch('/stopinterval'); setTimeout(function(){location.reload();},500);}";
    html += "</script>";
  }
  
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

void handleTrigger() {
  triggerShutter();
  server.send(200, "text/plain", "Shutter triggered");
}

void handleStartInterval() {
  intervalRunning = true;
  shotsTaken = 0;
  lastShotTime = 0;
  server.send(200, "text/plain", "Interval started");
}

void handleStopInterval() {
  intervalRunning = false;
  server.send(200, "text/plain", "Interval stopped");
}

void handleSetMode() {
  if (server.hasArg("mode")) {
    String mode = server.arg("mode");
    if (mode == "button") currentMode = MODE_BUTTON;
    else if (mode == "sensor1") currentMode = MODE_SENSOR1;
    else if (mode == "sensor2") currentMode = MODE_SENSOR2;
    else if (mode == "interval") {
      currentMode = MODE_INTERVAL;
      intervalRunning = false;
    }
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSetSettings() {
  if (server.hasArg("delay")) pressDelay = server.arg("delay").toInt();
  if (server.hasArg("duration")) pressDuration = server.arg("duration").toInt();
  
  unsigned long minInterval = pressDelay + pressDuration;
  if (server.hasArg("interval")) {
    interval = server.arg("interval").toInt();
    if(interval < minInterval) interval = minInterval;
  }
  
  if (server.hasArg("count")) shotCount = server.arg("count").toInt();
  
  unsigned long minTimeout = minInterval / 1000 + 1;
  if (server.hasArg("timeout")) {
    sensorTimeout = server.arg("timeout").toInt() * 1000;
    if(sensorTimeout < minTimeout * 1000) sensorTimeout = minTimeout * 1000;
  }
  
  if(currentMode == MODE_SENSOR1) {
    sensor1Inverted = server.hasArg("invert");
  } else if(currentMode == MODE_SENSOR2) {
    sensor2Inverted = server.hasArg("invert");
  }
  
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleResetStats() {
  shotsTaken = 0;
  totalShots = 0;
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleGetSettings() {
  String response = "delay=" + String(pressDelay);
  response += "&duration=" + String(pressDuration);
  response += "&timeout=" + String(sensorTimeout/1000);
  response += "&s1inv=" + String(sensor1Inverted ? "1" : "0");
  response += "&s2inv=" + String(sensor2Inverted ? "1" : "0");
  server.send(200, "text/plain", response);
}

void triggerShutter() {
  if (isShooting) return;
  
  isShooting = true;
  shotsTaken++;
  totalShots++;
  
  delay(pressDelay);
  digitalWrite(SHUTTER_PIN, HIGH);
  delay(pressDuration);
  digitalWrite(SHUTTER_PIN, LOW);
  
  isShooting = false;
}

bool checkSensorDebounced(int pin, bool inverted, bool &lastState, unsigned long &lastDebounceTime) {
  bool reading = digitalRead(pin);
  if (inverted) reading = !reading;
  
  if (reading != lastState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (reading != lastState) {
      lastState = reading;
      return true;
    }
  }
  return false;
}

void handleSensor(int pin, bool inverted, bool &lastState, unsigned long &lastDebounceTime) {
  if (checkSensorDebounced(pin, inverted, lastState, lastDebounceTime)) {
    if (lastState == LOW) {
      if (!sensorActive) {
        sensorActive = true;
        sensorActiveStart = millis();
        triggerShutter();
      }
    } else {
      sensorActive = false;
    }
  }
  
  if (sensorActive && (millis() - sensorActiveStart > sensorTimeout)) {
    sensorActive = false;
  }
}

// Вернем предыдущую обработку сенсоров
void checkSensor(int pin, bool inverted) {
  bool sensorState = digitalRead(pin);
  if(inverted) sensorState = !sensorState;
  
  if (sensorState == LOW) {
    if (!sensorActive) {
      sensorActive = true;
      sensorActiveStart = millis();
      triggerShutter();
    } else if (millis() - sensorActiveStart > sensorTimeout) {
      return;
    }
  } else {
    sensorActive = false;
  }
}

void setup() {
  
  pinMode(LED_BUILTINN, OUTPUT);
  pinMode(SHUTTER_PIN, OUTPUT);
  pinMode(SENSOR1_PIN, INPUT_PULLUP); // D6 с подтяжкой к питанию
  pinMode(SENSOR2_PIN, INPUT);        // D7 без подтяжки
  digitalWrite(SHUTTER_PIN, LOW);
  
  for(int i=0;i<5;i++){
  digitalWrite(LED_BUILTINN, LOW);
  delay(500);
  digitalWrite(LED_BUILTINN, HIGH);
  delay(500); 
  }

  WiFi.softAP(ssid, password);
  
  server.on("/", handleRoot);
  server.on("/trigger", handleTrigger);
  server.on("/startinterval", handleStartInterval);
  server.on("/stopinterval", handleStopInterval);
  server.on("/setmode", handleSetMode);
  server.on("/setsettings", handleSetSettings);
  server.on("/resetstats", handleResetStats);
  server.on("/settings", handleGetSettings);
  
  server.begin();
  Serial.begin(115200);
  Serial.println("Server started");
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
}

void loop() {
  server.handleClient();
  
  switch(currentMode) {
    case MODE_SENSOR1:
       checkSensor(SENSOR1_PIN, sensor1Inverted);
      break;
      
    case MODE_SENSOR2:
      checkSensor(SENSOR2_PIN, sensor2Inverted);
      break;
      
    case MODE_INTERVAL:
      if (intervalRunning && !isShooting && (shotCount == 0 || shotsTaken < shotCount) && millis() - lastShotTime >= interval) {
        triggerShutter();
        lastShotTime = millis();
      }
      break;
      
    case MODE_BUTTON:
    default:
      break;
  }
  
  delay(10);
}