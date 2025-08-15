# RunoutAnywhere Klipper Edition (ESP8266/ESP32)

**WiFi Filament Runout Sensor for Klipper/Moonraker**

WiFi filament runout sensor for Klipper/Moonraker, now with support for sending the `M600` G-code command on filament runout. The M600 command allows you to use Klipper's macro handling for filament change events, giving you full control of the printer's behavior after runout.

---

## Features

- **Web GUI for configuration**  
  - Set Moonraker API URL and API Key
  - Save and reboot device from browser

- **LED Status Codes**  
  - Visual feedback for filament presence, runout, comms error, etc.

- **Persistent Settings**  
  - Stores API info in EEPROM

- **Klipper/Moonraker Integration**  
  - Sends `M600` to Klipper via HTTP when runout detected
  - Sends resume command on filament reload (optional)

- **Live Device Status**  
  - Web GUI shows WiFi, API, filament, and device state

- **Simple Endstop Support**  
  - Use any NO/NC switch for filament runout sensor

- **Easy ESP32 Adaptation**  
  - Minor code changes for ESP32 compatibility (see source comments)

---

## How It Works

1. **Startup & Configuration**
   - Device connects to configured WiFi.
   - Settings (Moonraker API URL, API key) are loaded from EEPROM.
   - A minimal web server is started on port 80 for configuration and status.

2. **Filament Sensor Monitoring**
   - The sensor pin is checked every 50ms.
   - When filament is present, LED is solid ON.
   - When filament is absent (runout), LED blinks rapidly.

3. **On Filament Runout**
   - Device detects a change from present to runout.
   - LED blinks rapidly as a visual alert.
   - Device sends an HTTP POST to the Moonraker API endpoint:
     ```
     POST /printer/gcode/script
     Content-Type: application/json
     X-Api-Key: {your_api_key}
     Body: {"script":"M600"}
     ```
   - This triggers the `[gcode_macro M600]` in your Klipper `printer.cfg` file, allowing you to control what happens next (e.g., pause, park, beep, wait for reload, etc.).
   - If the API call fails, LED blinks slowly 3 times as an error signal.

4. **On Filament Reload**
   - Device detects filament is loaded again.
   - If `auto_resume` is enabled, sends `/printer/print/resume` to Moonraker.
   - LED blinks twice to indicate reload.

5. **Web GUI**
   - Visit the device IP in a browser to:
     - Change Moonraker server address and API key
     - View live status (WiFi, Moonraker, filament, last action)
     - See LED status code chart

---

## Wiring

- **Sensor Pin:** Connect your filament runout switch (NO/NC) to D2 (NodeMCU/Wemos D1 Mini, GPIO4).
- **LED Pin:** Onboard LED is D4 (GPIO2, active LOW).

---

## Example Klipper Macro (`printer.cfg`)

```ini
[gcode_macro M600]
gcode:
  PAUSE
  G91
  G1 Z10 F1000
  G90
  # Add additional script for filament change, beep, move, etc.
```

---

## Installation & Flashing

1. Flash the firmware to your ESP8266/ESP32 (using Arduino IDE or PlatformIO).
2. Connect the board to your printer’s runout switch and power.
3. Connect to WiFi (SSID and password are set in the source code).
4. Visit the device's IP address in a browser to configure Moonraker API details.

---

## LED Status Table

| Status                         | LED Behavior         |
|---------------------------------|---------------------|
| Filament present                | Solid ON            |
| Runout (while printing)         | Rapid blinking      |
| No filament (idle)              | OFF, short blink    |
| Filament reloaded/ready         | ON, short blink     |
| Comms/API error                 | Slow even blinking  |

---

## Notes

- The device uses a simple debounce logic to avoid false triggers.
- EEPROM is used for persistent settings.
- All status and actions are visible in the Web GUI.
- For ESP32, use `WiFi.h` and `WebServer` (see comments in `.ino`).

---

## License

&copy; 2025 RunoutAnywhere Klipper

