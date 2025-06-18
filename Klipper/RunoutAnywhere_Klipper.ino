/**
   RunoutAnywhere for Klipper (Moonraker API)
   Adapted from original Octoprint version by Robert W. Mech

   This version monitors a filament runout sensor and communicates via Moonraker API
   to Klipper, pausing or resuming prints as needed.

   LED STATUSES
   ------------
   (same behavior as original, see original code)

   Required Libraries:
    - ESP8266WiFi
    - ArduinoJson
    - ESP8266HTTPClient
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>

// CONFIGURATION
#define CHECK_INTERVAL                   2000
#define LED_PIN                          2
#define SENSOR_PIN                       4
#define AUTO_RESUME                      false
#define LINES_BEFORE_HEADER              20
#define SERIAL_RATE                      115200
const char* SSID                   =     "YOUR_SSID";
const char* WIFI_PASSWORD          =     "YOUR_PASS";
const char* MOONRAKER_API_KEY      =     ""; // Only if your Moonraker requires an API key
const char* MOONRAKER_URL          =     "http://192.168.10.112:7125"; // Moonraker default port is 7125

// END CONFIGURATION

ESP8266WiFiMulti WiFiMulti;
boolean connectioWasAlive = true;
boolean ledToggle = false;
boolean printerPaused = false;
int runoutSensor = 0;
DynamicJsonDocument doc(2048); // Klipper returns more data, so increased buffer
// Printer status flags
bool state_printing = false;
bool state_ready = false;
bool state_paused = false;
const char* state_text;
int linesPrinted = 0;
int httpCode = 0;
String payload = "";
HTTPClient http;

void setup() {
  Serial.begin(SERIAL_RATE);
  pinMode(LED_PIN, OUTPUT);
  pinMode(SENSOR_PIN, INPUT_PULLUP);
  showBannerMessage();

  Serial.println("Pausing for WiFi Startup...\n\n");
  for (int i = 0; i < 4; i++) {
    Serial.flush();
    delay(1000);
  }
  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(SSID, WIFI_PASSWORD);
}

void loop() {
  monitorWiFi();

  if ((WiFiMulti.run() == WL_CONNECTED)) {
    WiFiClient client;
    showHeader();

    // Query Moonraker for printer object status
    String statusUrl = String(MOONRAKER_URL) + "/printer/objects/query?print_stats&virtual_sdcard";
    if (http.begin(client, statusUrl)) {
      // Moonraker API key (rarely needed by default, but add header if you use one)
      if (strlen(MOONRAKER_API_KEY) > 0) {
        http.addHeader("X-Api-Key", MOONRAKER_API_KEY);
      }
      http.addHeader("Content-Type", "application/json");

      httpCode = http.GET();
      checkRunoutSwitchStatus();

      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        payload = http.getString();
        http.end();

        DeserializationError error = deserializeJson(doc, payload);
        if (error) {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.c_str());
          return;
        } else {
          // Extract relevant state from Moonraker
          JsonObject print_stats = doc["result"]["status"]["print_stats"];
          String state = print_stats["state"] | "";
          state_printing = (state == "printing");
          state_paused   = (state == "paused");
          state_ready    = (state == "standby" || state == "ready" || state == "complete"); // standby may mean ready to print
          state_text = state.c_str();

          writeStatusLine();

          // Only act if printing or paused
          if (state_printing || state_paused) {
            checkRunoutSwitchStatus();
            if (runoutSensor == HIGH) { // Filament OUT
              // Flash LED
              for (int i = 0; i < 10; i++) {
                delay(75);
                digitalWrite(LED_PIN, LOW);
                delay(75);
                digitalWrite(LED_PIN, HIGH);
              }
              // Only pause if not already paused
              if (!state_paused) {
                String pauseUrl = String(MOONRAKER_URL) + "/printer/print/pause";
                if (http.begin(client, pauseUrl)) {
                  if (strlen(MOONRAKER_API_KEY) > 0) {
                    http.addHeader("X-Api-Key", MOONRAKER_API_KEY);
                  }
                  http.addHeader("Content-Type", "application/json");
                  httpCode = http.POST("{}"); // No payload needed for pause
                  payload = http.getString();
                  writeStatusLine();
                  http.end();
                }
              }
            } else { // Filament IN
              digitalWrite(LED_PIN, LOW);
              if (state_paused && AUTO_RESUME == true) {
                String resumeUrl = String(MOONRAKER_URL) + "/printer/print/resume";
                if (http.begin(client, resumeUrl)) {
                  if (strlen(MOONRAKER_API_KEY) > 0) {
                    http.addHeader("X-Api-Key", MOONRAKER_API_KEY);
                  }
                  http.addHeader("Content-Type", "application/json");
                  httpCode = http.POST("{}"); // No payload needed for resume
                  payload = http.getString();
                  writeStatusLine();
                  http.end();
                }
              }
            }
          } else {
            printerPaused = false;
          }
        }
      } else {
        writeStatusLine();
      }
      http.end();
    } else {
      Serial.printf("[HTTP] Unable to connect\n");
    }
  }

  checkRunoutSwitchStatus();
  ledActions();
  delay(CHECK_INTERVAL);
}

void ledActions() {
  // LED blinking logic same as before
  if (state_ready == true) {
    if (runoutSensor == HIGH) {
      digitalWrite(LED_PIN, LOW);
      delay(50);
      digitalWrite(LED_PIN, HIGH);
    } else {
      digitalWrite(LED_PIN, HIGH);
      delay(50);
      digitalWrite(LED_PIN, LOW);
    }
  }
  if (state_ready == false && state_paused == false && state_printing == false) {
    if (ledToggle) {
      ledToggle = false;
      digitalWrite(LED_PIN, LOW);
    } else {
      ledToggle = true;
      digitalWrite(LED_PIN, HIGH);
    }
  }
}

void monitorWiFi()
{
  if (WiFiMulti.run() != WL_CONNECTED)
  {
    if (connectioWasAlive == true)
    {
      connectioWasAlive = false;
      Serial.print("Looking for WiFi ");
    }
    Serial.print(".");
    delay(500);
  }
  else if (connectioWasAlive == false)
  {
    connectioWasAlive = true;
    Serial.printf(" connected to %s\n", WiFi.SSID().c_str());
  }
}

void showHeader() {
  if (linesPrinted == 0 || linesPrinted > LINES_BEFORE_HEADER) {
    Serial.println("\n*******************************************************************************");
    Serial.println("** RunoutAnywhere System Status    (1=True, 0=False)                         **");
    Serial.println("*******************************************************************************");
    Serial.println("Ready   Printing  Paused   Filament   HTTP & Server Response                   ");
    Serial.println("-----   --------  ------   --------   -----------------------------------------");
    linesPrinted = 0;
  }
}

void writeStatusLine() {
  Serial.printf(   "%d       %d         %d        %d          ", state_ready, state_printing, state_paused, runoutSensor == LOW);
  Serial.print(httpCode);
  Serial.print(" ");
  Serial.println(state_text);
  linesPrinted++;
}

void checkRunoutSwitchStatus() {
  runoutSensor = digitalRead(SENSOR_PIN);
}

void showBannerMessage() {
  Serial.println();
  Serial.println("*******************************************************************************");
  Serial.println("* RunoutAnywhere (Klipper) - Adapted from Robert W. Mech   www.MakersMashup.com *");
  Serial.println("*******************************************************************************");
  Serial.println();
}
