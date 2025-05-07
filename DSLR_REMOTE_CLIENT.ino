#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

#define SERVER_SSID "DSLR_Remote"
#define SERVER_PASSWORD "12345678"
#define SERVER_IP "192.168.4.1"
#define SERVER_PORT 80

#define SENSOR1_PIN D6
#define SENSOR2_PIN D7
#define SHUTTER_PIN D2
#define LED_BUILTINN D4
#define SETTINGS_UPDATE_INTERVAL 30000
#define DEBOUNCE_DELAY 50

struct Settings {
  unsigned long pressDelay;
  unsigned long pressDuration;
  bool sensor1Inverted;
  bool sensor2Inverted;
} currentSettings;

unsigned long lastSettingsUpdate = 0;
bool lastSensor1State = HIGH;
bool lastSensor2State = HIGH;
unsigned long lastTriggerTime = 0;
bool shutterActive = false;

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTINN, OUTPUT);
  pinMode(SENSOR1_PIN, INPUT_PULLUP);
  pinMode(SENSOR2_PIN, INPUT);
  pinMode(SHUTTER_PIN, OUTPUT);
  digitalWrite(SHUTTER_PIN, LOW);
  

  for(int i=0;i<5;i++){
  digitalWrite(LED_BUILTINN, LOW);
  delay(500);
  digitalWrite(LED_BUILTINN, HIGH);
  delay(500);
  }

  connectToWiFi();
}

void connectToWiFi() {
  WiFi.begin(SERVER_SSID, SERVER_PASSWORD);
  Serial.println("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected!");
}

void updateSettings() {
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
    return;
  }

  WiFiClient client;
  HTTPClient http;
  http.begin(client, "http://" SERVER_IP "/settings");
  
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    parseSettings(payload);
    Serial.println("Settings updated");
  }
  http.end();
}

void parseSettings(String payload) {
  // Формат: delay=100&duration=100&s1inv=0&s2inv=1
  currentSettings.pressDelay = getValue(payload, "delay=", '&').toInt();
  currentSettings.pressDuration = getValue(payload, "duration=", '&').toInt();
  currentSettings.sensor1Inverted = getValue(payload, "s1inv=", '&').toInt() == 1;
  currentSettings.sensor2Inverted = getValue(payload, "s2inv=", '&').toInt() == 1;
}

String getValue(String data, String prefix, char separator) {
  int start = data.indexOf(prefix);
  if (start < 0) return "";
  start += prefix.length();
  int end = data.indexOf(separator, start);
  if (end < 0) end = data.length();
  return data.substring(start, end);
}

void triggerServerShutter() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClient client;
  HTTPClient http;
  http.begin(client, "http://" SERVER_IP "/trigger");
  http.GET();
  http.end();
}

void handleSensor(int pin, bool inverted, bool &lastState) {
  bool currentState = digitalRead(pin);
  if (inverted) currentState = !currentState;
  
  if (currentState != lastState) {
    delay(DEBOUNCE_DELAY); // Простая защита от дребезга
    currentState = digitalRead(pin);
    if (inverted) currentState = !currentState;
    
    if (currentState != lastState) {
      lastState = currentState;
      
      if (currentState == LOW) {
        // Нажатие сенсора
        digitalWrite(SHUTTER_PIN, HIGH);
        triggerServerShutter();
        Serial.println("Sensor activated - triggering");
      } else {
        // Отпускание сенсора
        digitalWrite(SHUTTER_PIN, LOW);
        Serial.println("Sensor released");
      }
    }
  }
}

void loop() {
  if (millis() - lastSettingsUpdate > SETTINGS_UPDATE_INTERVAL) {
    updateSettings();
    lastSettingsUpdate = millis();
  }
  
  handleSensor(SENSOR1_PIN, currentSettings.sensor1Inverted, lastSensor1State);
  handleSensor(SENSOR2_PIN, currentSettings.sensor2Inverted, lastSensor2State);
  
  delay(10);
}