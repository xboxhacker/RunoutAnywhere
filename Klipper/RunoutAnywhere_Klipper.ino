/*
 * RunoutAnywhere Klipper Edition (ESP8266/ESP32)
 * WiFi filament runout sensor for Klipper/Moonraker
 * Features:
 *  - Web GUI for configuration (Moonraker URL, API key)
 *  - LED status codes (with table in web UI)
 *  - Persistent settings (EEPROM)
 *  - Sends pause/resume to Klipper via HTTP
 *  - Live device status in web GUI
 *  - Save and reboot via web GUI
 *  - Simple endstop (NO/NC) as filament runout switch
 *  - Easy to adapt for ESP32 (use WiFi.h, WebServer)
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ESP8266HTTPClient.h>

// ----------------------------------------------
// --- User Config (WiFi credentials)
// ----------------------------------------------
const char* SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// --- Pin Configuration ---
#define SENSOR_PIN D2     // Your filament runout switch (GPIO4 on NodeMCU/Wemos D1 Mini)
#define LED_PIN    D4     // Onboard LED (GPIO2 on NodeMCU/Wemos D1 Mini, active LOW)

// --- EEPROM Layout ---
#define EEPROM_SIZE 512
#define EEPROM_MOONRAKER_URL_ADDR   0
#define EEPROM_API_KEY_ADDR         100

// --- Global Variables (settings and state) ---
String moonraker_url = "http://192.168.1.100:7125";
String moonraker_api_key = "";
String status_message = "Idle";
String last_action = "None";
String filament_status = "Present"; // "Present" or "Runout"
String wifi_status = "Disconnected";
String moonraker_status = "Disconnected";
bool filament_present = true;
bool auto_resume = false; // Change to true for auto-resume mode

ESP8266WebServer server(80);

// ---- Load and Save settings ----
void saveSettings() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.writeString(EEPROM_MOONRAKER_URL_ADDR, moonraker_url);
  EEPROM.writeString(EEPROM_API_KEY_ADDR, moonraker_api_key);
  EEPROM.commit();
  EEPROM.end();
}

void loadSettings() {
  EEPROM.begin(EEPROM_SIZE);
  String url = EEPROM.readString(EEPROM_MOONRAKER_URL_ADDR);
  if (url.length() > 5) moonraker_url = url;
  String apikey = EEPROM.readString(EEPROM_API_KEY_ADDR);
  if (apikey.length() > 3) moonraker_api_key = apikey;
  EEPROM.end();
}

// ---- Web GUI ----
String htmlPage() {
  String html = "<html><head><title>RunoutAnywhere Klipper Config</title>";
  html += "<style>body{font-family:sans-serif;}label{display:block;margin-top:10px;}input[type=text]{width:80%;}table{border-collapse:collapse;}td,th{border:1px solid #888;padding:4px 8px;}th{background:#eee;}</style>";
  html += "</head><body><h2>RunoutAnywhere Klipper ESP8266</h2>";

  html += "<form method='POST' action='/save'>";
  html += "<label>Moonraker API Server Address:</label>";
  html += "<input type='text' name='moonraker_url' value='" + moonraker_url + "'><br/>";
  html += "<label>Moonraker API Key:</label>";
  html += "<input type='text' name='moonraker_api_key' value='" + moonraker_api_key + "'><br/>";
  html += "<br/><button type='submit'>Save and Reboot</button>";
  html += "</form>";

  html += "<hr><h3>LED Status Codes</h3>";
  html += "<table>";
  html += "<tr><th>Status</th><th>LED Behavior</th></tr>";
  html += "<tr><td>Filament present</td><td>Solid ON</td></tr>";
  html += "<tr><td>Runout (while printing)</td><td>Rapid blinking</td></tr>";
  html += "<tr><td>No filament (idle)</td><td>OFF, short blink</td></tr>";
  html += "<tr><td>Filament reloaded/ready</td><td>ON, short blink</td></tr>";
  html += "<tr><td>Comms/API error</td><td>Slow even blinking</td></tr>";
  html += "</table>";

  html += "<hr><h3>Live Device Status</h3>";
  html += "<ul>";
  html += "<li><b>WiFi:</b> " + wifi_status + "</li>";
  html += "<li><b>Moonraker:</b> " + moonraker_status + "</li>";
  html += "<li><b>Filament:</b> " + filament_status + "</li>";
  html += "<li><b>Last Action:</b> " + last_action + "</li>";
  html += "<li><b>Status Message:</b> " + status_message + "</li>";
  html += "</ul>";

  html += "<hr><small>&copy; 2025 RunoutAnywhere Klipper</small>";
  html += "</body></html>";
  return html;
}

void handleRoot() {
  server.send(200, "text/html", htmlPage());
}

void handleSave() {
  if (server.method() == HTTP_POST) {
    if (server.hasArg("moonraker_url")) moonraker_url = server.arg("moonraker_url");
    if (server.hasArg("moonraker_api_key")) moonraker_api_key = server.arg("moonraker_api_key");
    saveSettings();
    server.send(200, "text/html", "<html><body><h2>Settings Saved. Rebooting...</h2></body></html>");
    delay(1000);
    ESP.restart();
  } else {
    server.send(405, "text/plain", "Method Not Allowed");
  }
}

void handleStatus() {
  String json = "{";
  json += "\"wifi_status\":\"" + wifi_status + "\",";
  json += "\"moonraker_status\":\"" + moonraker_status + "\",";
  json += "\"filament_status\":\"" + filament_status + "\",";
  json += "\"last_action\":\"" + last_action + "\",";
  json += "\"status_message\":\"" + status_message + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

// ---- LED Status Codes ----
void ledOn()   { digitalWrite(LED_PIN, LOW); }
void ledOff()  { digitalWrite(LED_PIN, HIGH); }
void ledBlink(int times, int on_ms, int off_ms) {
  for (int i = 0; i < times; i++) {
    ledOn(); delay(on_ms);
    ledOff(); delay(off_ms);
  }
}

void updateLED() {
  // Example, minimal logic: solid on if present, rapid blink if runout
  if (filament_present) {
    ledOn();
  } else {
    ledBlink(3, 100, 100);
  }
}

// ---- Moonraker Communication ----
bool sendMoonrakerPause() {
  HTTPClient http;
  String url = moonraker_url + "/printer/print/pause";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  if (moonraker_api_key.length() > 0)
    http.addHeader("X-Api-Key", moonraker_api_key);

  int httpCode = http.POST("{}");
  bool ok = (httpCode == 204 || httpCode == 200);
  http.end();
  moonraker_status = ok ? "Paused" : "API Error";
  last_action = "Pause";
  status_message = ok ? "Pause sent to Moonraker" : "Pause failed";
  return ok;
}

bool sendMoonrakerResume() {
  HTTPClient http;
  String url = moonraker_url + "/printer/print/resume";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  if (moonraker_api_key.length() > 0)
    http.addHeader("X-Api-Key", moonraker_api_key);

  int httpCode = http.POST("{}");
  bool ok = (httpCode == 204 || httpCode == 200);
  http.end();
  moonraker_status = ok ? "Resumed" : "API Error";
  last_action = "Resume";
  status_message = ok ? "Resume sent to Moonraker" : "Resume failed";
  return ok;
}

// ---- Filament Sensor Logic ----
bool last_filament_present = true;
unsigned long last_action_time = 0;
const unsigned long debounce_ms = 200;

void checkFilament() {
  bool now_present = digitalRead(SENSOR_PIN) == LOW; // LOW = present (NC switch to GND)
  filament_present = now_present;
  filament_status = now_present ? "Present" : "Runout";

  if (now_present != last_filament_present && millis() - last_action_time > debounce_ms) {
    last_action_time = millis();
    if (!now_present) { // Runout
      status_message = "Filament runout detected";
      ledBlink(10, 100, 100); // Rapid flash
      bool ok = sendMoonrakerPause();
      if (!ok) { ledBlink(3, 500, 500); } // Signal error
    } else { // Filament reloaded
      status_message = "Filament reloaded";
      if (auto_resume) {
        bool ok = sendMoonrakerResume();
        if (ok) ledBlink(2, 300, 100);
        else   ledBlink(3, 500, 500);
      } else {
        ledBlink(2, 300, 100);
      }
    }
    last_filament_present = now_present;
  }
}

// ---- Arduino Setup & Loop ----
void setup() {
  pinMode(SENSOR_PIN, INPUT_PULLUP); // Use pullup for NO/NC switches
  pinMode(LED_PIN, OUTPUT);
  ledOff();

  Serial.begin(115200);
  loadSettings();
  status_message = "Connecting WiFi...";
  WiFi.begin(SSID, WIFI_PASSWORD);
  wifi_status = "Connecting...";
  int wait = 0;
  while (WiFi.status() != WL_CONNECTED && wait < 100) {
    delay(100);
    Serial.print(".");
    wait++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    wifi_status = WiFi.localIP().toString();
    status_message = "WiFi connected";
  } else {
    wifi_status = "WiFi error";
    status_message = "WiFi failed";
  }

  // Setup web server
  server.on("/", handleRoot);
  server.on("/save", handleSave);
  server.on("/status", handleStatus);
  server.begin();
  status_message = "Web GUI Ready";
  ledBlink(3, 100, 300);
}

void loop() {
  server.handleClient();
  checkFilament();
  updateLED();
  delay(50); // Adjust for responsiveness
}
