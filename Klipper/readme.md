# RunoutAnywhere Klipper Edition

**WiFi Filament Runout Sensor for Klipper/Moonraker**

---

## Overview

**RunoutAnywhere_Klipper.ino** is an ESP8266/ESP32-based WiFi filament runout sensor solution that works with [Klipper](https://www.klipper3d.org/) via the [Moonraker](https://moonraker.readthedocs.io/) API.  
It is inspired by the original [RunoutAnywhere](https://bitbucket.org/rwmech/runoutanywhere/src/master/) project for Octoprint, but adapted for Klipper firmware.

- Uses a simple endstop or microswitch as the runout sensor.
- Sends pause/resume commands to Klipper over WiFi when filament runs out or is reloaded.
- No need to wire the sensor directly to the printer controller—ideal for remote sensors or multi-printer setups.

---

## Features

- **Wireless:** Use anywhere on your printer, enclosure, or even in a drybox.
- **Klipper/Moonraker Integration:** Communicates with Klipper using HTTP requests.
- **LED Status:** Visual feedback for filament present, runout, communication status, and errors.
- **Optional Auto-Resume:** Optionally resumes printing automatically when filament is reloaded.
- **Simple Hardware:** Just an ESP8266/ESP32 and a switch.

---

## Hardware Requirements

- ESP8266 (NodeMCU, Wemos D1 Mini, etc.) or ESP32 (with minor code tweaks).
- Filament runout switch (normally closed or normally open).
- Status LED (optional, for visual feedback).
- 3D printer running Klipper with Moonraker API (Fluidd/Mainsail compatible).

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
   - Set your Moonraker API server address (e.g., `http://192.168.10.112:7125`).
   - (Optional) Set your Moonraker API key if required.
   - Choose `AUTO_RESUME` as desired.

2. **Flash the ESP8266:**
   - Use PlatformIO, Arduino IDE, or ESPTool.

3. **Configure Klipper:**
   - Use the included `filament_runout_wifi.cfg` macros.
   - Add to your `printer.cfg`:
     ```
     [include filament_runout_wifi.cfg]
     ```
   - These macros handle pausing, parking, saving position, timeouts, and resuming safely.

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

## Advanced: Macro Features

- **Remembers last position and movement mode on pause.**
- **Parks head and lifts Z.**
- **10-minute (configurable) timeout – turns off hotend if not resumed.**
- **On resume, restores hotend temp and waits for it to reheat before resuming.**
- **Easily adjust timeout with `SET_FILAMENT_TIMEOUT TIMEOUT=900` (seconds).**

See the supplied `filament_runout_wifi.cfg` for details.

---

## Customization

- Change `LED_PIN` and `SENSOR_PIN` as needed for your wiring.
- Edit parking coordinates in the `PAUSE` macro (`G1 X10 Y10 F6000`).
- For multiple printers, use unique ESP8266 devices and configure separate macros if needed.

---

## Troubleshooting

- If Klipper doesn't pause, check your ESP8266's WiFi and Moonraker URL/API key.
- Use the serial monitor for debug output.
- Make sure your firewall allows Moonraker API access from the ESP8266.
- Find your Moonraker API Key: `http://{your-moonraker-host}/access/api_key`

---

## Credits

- Original concept and code: [Robert W. Mech](https://www.makersmashup.com/)

---

## License

BSD 3-Clause License (see original source).  
