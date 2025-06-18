# RunoutAnywhere

**A Universal WiFi Filament Runout Sensor for OctoPrint**

---

## Overview

**RunoutAnywhere** is an ESP8266-based WiFi filament runout sensor designed to work seamlessly with [OctoPrint](https://octoprint.org/) via its REST API.  
Inspired by the need for remote and reliable filament monitoring, this project allows you to place the sensor anywhere—on your printer, enclosure, or even in your filament drybox.

- Uses a simple endstop or microswitch as the runout sensor.
- Sends pause/resume commands to OctoPrint over WiFi when filament runs out or is reloaded.
- No need to wire the sensor directly to the printer controller.

---

## Features

- **Wireless:** Place the runout sensor anywhere, no need for physical connection to the printer board.
- **OctoPrint Integration:** Communicates with OctoPrint using HTTP REST API.
- **LED Status:** Visual feedback for filament present, runout, communication status, and errors.
- **Optional Auto-Resume:** Optionally resumes printing automatically when filament is reloaded.
- **Simple Hardware:** Just an ESP8266 and a switch.

---

## Hardware Requirements

- ESP8266 (NodeMCU, Wemos D1 Mini, etc.)
- Filament runout switch (normally closed or normally open)
- Status LED (optional, for visual feedback)
- 3D printer running OctoPrint (any board supported by OctoPrint)

---

## Wiring

| ESP8266 Pin | Function          | Description                  |
|-------------|-------------------|------------------------------|
| GPIO2       | LED_PIN           | Status LED (optional)        |
| GPIO4       | SENSOR_PIN        | Filament runout switch input |

- Connect one side of the switch to `GND`, the other to `GPIO4`.
- LED (with resistor) from `GPIO2` to `GND`.

---

## Installation & Setup

1. **Edit the Code:**
   - Set your WiFi SSID and password.
   - Set your OctoPrint server address and API key.
   - Choose `AUTO_RESUME` as desired.

2. **Flash the ESP8266:**
   - Use PlatformIO, Arduino IDE, or ESPTool.

3. **Configure OctoPrint:**
   - No plugins required, but ensure API access is enabled and the correct API key is set.

4. **Wiring:**
   - Connect switch and LED as described above.

5. **Test:**
   - Start a print, remove filament to trigger pause, reinsert and resume.

---

## LED Status Indicators

| Status                  | LED Behavior                   |
|-------------------------|-------------------------------|
| Filament present        | Solid ON                      |
| Runout (printing)       | Rapid flashing                |
| No filament (idle)      | Off, short blink              |
| Filament loaded, ready  | On, short blink               |
| Comms error             | Slow even blinking            |

---

## Example Configuration

Edit these lines in the code for your setup:
```cpp
const char* SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* OCTOPRINT_API_KEY = "YOUR_OCTOPRINT_API_KEY";
const char* OCTOPRINT_BASE_URL = "http://octoprint.local:5000/";
#define AUTO_RESUME false // or true, as desired
```

---

## Troubleshooting

- If OctoPrint doesn't pause, check your ESP8266's WiFi and API key.
- Use the serial monitor for debug output.
- Make sure your firewall allows OctoPrint API access from the ESP8266.

---

## References

- Original author: [Robert W. Mech](https://www.makersmashup.com/)
- Published on: [MakersMashup YouTube Channel](https://www.youtube.com/channel/UCevttltfkZ76jwqfV1vrRbQ)
- Project Homepage: [RunoutAnywhere Bitbucket](https://bitbucket.org/rwmech/runoutanywhere/src/master/)

---

## License

BSD 3-Clause License (see source code for details).
