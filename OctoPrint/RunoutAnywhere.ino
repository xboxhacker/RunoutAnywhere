

/**
   RunoutAnywhere.ino

   https://bitbucket.org/rwmech/runoutanywhere/src/master/

   RunoutAnywhere is a runout filament sensor that communicates via REST API's to Octoprint/Klipper and other control interfaces.
   Simply add one end stop switch that closes when filament is not passing over it.  This triggers the runout and causes
   Octoprint to pause printing.  Once the filament has been put back through, printing will resume (if enabled).

   Submit Issues to : https://bitbucket.org/rwmech/runoutanywhere/issues

   LED STATUSES
   ------------
   Solid LED                : Filament in place and printing.
   Rapid flashing           : Filament runout and printing is paused.
   LED Off with short blink : No filament loaded but communicating with Octoprint and not printing
   LED ON with short blink  : Filament loaded and communicating with Octoprint ready to print
   Slow even blinking       : Communication error with octoprint (configuration bad)

   Published on www.MakersMashup.com & available on my YouTube Channel https://www.youtube.com/channel/UCevttltfkZ76jwqfV1vrRbQ

   Copyright 2019 Robert W. Mech

   Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this
   software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.



*/
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>


/******************************************************************************************************

   CONFIGURATION

   Modify the following parameters for your use

*******************************************************************************************************/
#define CHECK_INTERVAL                   2000                                       // How long between checks 
#define LED_PIN                          2                                         // What pin do we use for the LED
#define SENSOR_PIN                       4                                         // Filament Sensor Pin 
#define AUTO_RESUME                      false                                       // Should we resume when filament is back in the sensor. If not you'll need to press resume in octoprint manually
#define LINES_BEFORE_HEADER              20                                         // every n lines we print the header to save garbage in the monitor window
#define SERIAL_RATE                      115200                                     // Speed of the serial debug window
const char* SSID                   =     "YOUR_SSID";                                // WiFi SSID
const char* WIFI_PASSWORD          =     "YOUR_PASS";                        		// WiFi Password
const char* OCTOPRINT_API_KEY      =     "YOUR_API_KEY";      					  // Octoprint API Key - Get this from the Octoprint admin panel under API
const char* OCTOPRINT_BASE_URL     =     "http://192.168.10.112:5000/";             // Octoprint Address and port

// END CONFIGURATION

// Main Variables
ESP8266WiFiMulti WiFiMulti;
boolean connectioWasAlive = true;
boolean ledToggle = false;
boolean printerPaused = false;
int runoutSensor = 0;
DynamicJsonDocument doc(1024); //1K buffer space
// JSON Object Status from Octoprint
bool state_flags_printing = false;
bool state_flags_ready = false;
bool state_flags_paused = false;
const char* state_text;
int linesPrinted = 0;
int httpCode = 0;
String payload = "";
HTTPClient http; // INIT HTTP Connection

void setup() {

  // Serial Debugging speed, messages are sent here
  Serial.begin(SERIAL_RATE);

  // Initialize the pins
  pinMode(LED_PIN, OUTPUT);          // LED output
  pinMode(SENSOR_PIN, INPUT_PULLUP); // initialize the filament runout switch pin as an input:

  showBannerMessage();

  Serial.println("Pausing for WiFi Startup...\n\n");
  for (int i = 0; i < 4; i++) {
    Serial.flush();
    delay(1000);
  }

  // Wifi Connection setup
  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(SSID, WIFI_PASSWORD);

}

void loop() {

  monitorWiFi(); // Check status

  // wait for WiFi connection
  if ((WiFiMulti.run() == WL_CONNECTED)) {

    WiFiClient client;

    // Header Display
    showHeader();

    // Check printer status and set to variables
    // Connection string to get printer status base URL plus the API URL, no history we just want the current status
    if (http.begin(client, String(OCTOPRINT_BASE_URL) + String("/api/printer?history=false"))) {

      // Octoprint Authentication Key is in Header, still insecure but better than plaintext on the URL.
      http.addHeader("X-Api-Key", OCTOPRINT_API_KEY);
      http.addHeader("Content-Type", "application/json");

      // start connection and send HTTP request
      httpCode = http.GET();

      checkRunoutSwitchStatus();  // Check the status of the filament runout switch.

      // Check for OK response code or print out the erro messages
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        payload = http.getString();
        http.end();  // Close connection after payload set

        // The deserialization should go fine here, if it does not there is a serious problem with octoprint or the buffer size above needs to be
        // increased.  Either way this is an edge case condition and we'll just dump out the error to the console.
        DeserializationError error = deserializeJson(doc, payload);  // Deserialize the JSON document
        // Test if parsing succeeds.
        if (error) {
          // Dump to screen.
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.c_str());
          return;
        } else {

          // Successful parsing of the octoprint JSON data, set the flags
          JsonObject state_flags = doc["state"]["flags"];
          state_flags_printing = state_flags["printing"];
          state_flags_ready = state_flags["ready"];
          state_flags_paused = state_flags["paused"];
          state_text = doc["state"]["text"]; // "Operational"

          writeStatusLine();

          // If printing is active check the runout sensor on SENSOR_PIN and see if it is still set to LOW
          // if it is set to HIGH then we need to post back to Octoprint to pause the print
          if (state_flags_printing || state_flags_paused) {

            // Sensor should always be in a high state meaning that the filament
            // is in place pushing the sensor on.  Light is lit.
            checkRunoutSwitchStatus();  // Check the status of the filament runout switch.

            // Runout Sensor Detection
            if (runoutSensor == HIGH) {
              // Show flashing LED Error
              for (int i = 0; i < 10; i++) {
                delay(75);
                digitalWrite(LED_PIN, LOW);
                delay(75);
                digitalWrite(LED_PIN, HIGH);
              }

              // digitalWrite(LED_PIN, HIGH);
              if (state_flags_paused == false) { // Only need to send the pause command if the printer is not already paused.
                int returnCode = http.begin(client, String(OCTOPRINT_BASE_URL) + String("/api/job"));
                http.addHeader("X-Api-Key", OCTOPRINT_API_KEY);
                http.addHeader("content-type", "application/json");  //Specify content-type header
                httpCode = http.POST("{\"command\": \"pause\",\"action\":\"pause\"}"); // Execute pause command to octoprint
                payload = http.getString();
                writeStatusLine();
                http.end();
              }
            } else {
              digitalWrite(LED_PIN, LOW);
              if (state_flags_paused && AUTO_RESUME == true) { // if AUTO_RESUME = false we skip this part.
                int returnCode = http.begin(String(OCTOPRINT_BASE_URL) + String("/api/job"));
                http.addHeader("X-Api-Key", OCTOPRINT_API_KEY);
                http.addHeader("content-type", "application/json");  //Specify content-type header
                httpCode = http.POST("{\"command\": \"pause\",\"action\":\"resume\"}"); // Execute resume command to octoprint
                payload = http.getString();
                writeStatusLine();
              }
            }
          } else {
            // Printer can never be paused if we're not printing
            printerPaused = false;
          } // Endif (state_flags_printing)

        } // endif deseralized JSON

      } else { // Error Condition Display some debugging details.

        writeStatusLine();

      } // Endif checking for http response codes

      http.end(); // Close this calls connection

    } else {
      Serial.printf("[HTTP} Unable to connect\n");
    }
  }

  checkRunoutSwitchStatus();  // Check the status of the filament runout switch.

  ledActions();

  delay(CHECK_INTERVAL);
}

/**
   LED Actions
   This method uses the current sensor and printing state to flash various information
   elimnating the need for a display (although you could add one)

*/
void ledActions() {
  // State LED Actions
  if (state_flags_ready == true) {
    // short blink = no filament, steady with short off = filament and ready
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

  if (state_flags_ready == false && state_flags_paused == false && state_flags_printing == false) {
    // Slow Blinking means we don't have a valid connection to octoprint or printer is not ready
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

// Show header
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

// Status line of activity to octoprint
void writeStatusLine() {
  Serial.printf(   "%d       %d         %d        %d          ", state_flags_ready, state_flags_printing, state_flags_paused, runoutSensor == LOW); // Invert sensor for display
  Serial.print(httpCode);
  if (httpCode == 403) {
    state_text = "Forbidden (Octoprint authorization failed)";
  }
  Serial.print(" ");
  Serial.println(state_text);
  linesPrinted++;
}

// Runout switch checking
void checkRunoutSwitchStatus() {
  runoutSensor = digitalRead(SENSOR_PIN);
}

// Banner Message
void showBannerMessage() {
  Serial.println();
  Serial.println("*******************************************************************************");
  Serial.println("* RunoutAnywhere - Copyright 2019 Robert W. Mech      www.MakersMashup.com    *");
  Serial.println("*******************************************************************************");
  Serial.println();
}
