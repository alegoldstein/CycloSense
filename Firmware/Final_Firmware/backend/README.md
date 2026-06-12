# Bike Computer Backend

Live dashboard + config server for the ESP32 bike computer.

## Architecture

```
ESP32  ──WS──▶  FastAPI server  ◀──WS──  Browser dashboard
                      │
                  REST /config
                  REST /history
```

- **ESP32** streams JSON telemetry every 250 ms to `/ws/device`
- **Browser** subscribes at `/ws/dashboard`, receives every packet in real-time
- **Config changes** flow: browser → server → ESP32 (pushed instantly over same WS)

## Server setup

```bash
cd bike-backend
pip install -r requirements.txt
uvicorn server:app --host 0.0.0.0 --port 8000 --reload
```

Open `http://<your-pc-ip>:8000` in a browser.

## ESP32 setup

1. Copy `ws_telemetry.h` and `ws_telemetry.cpp` into your PlatformIO `src/` folder.

2. Add to `platformio.ini`:
   ```ini
   lib_deps =
       links2004/WebSockets @ ^2.4.1
       bblanchon/ArduinoJson @ ^7.0.0
   ```

3. In `main.cpp`, after your other includes:
   ```cpp
   #include "ws_telemetry.h"
   ```

4. At the end of `setup()`, after WiFi credentials are known:
   ```cpp
   ws_telemetry_start("YOUR_SSID", "YOUR_PASSWORD", "192.168.1.100", 8000);
   //                                                 ^^^^ your PC's LAN IP
   ```

5. To make wheel circumference changes take effect without reboot, refactor
   `hall.cpp` to use a global instead of the compile-time macro:
   ```cpp
   // hall.cpp — near the top:
   float g_circumference_m = HALL_WHEEL_CIRCUMFERENCE_M;
   // Replace all uses of HALL_WHEEL_CIRCUMFERENCE_M in hall_get_speed_kmh()
   // and hall_poll_task() with g_circumference_m.
   ```
   Then in `ws_telemetry.cpp`, uncomment the line that sets `g_circumference_m`.

## Telemetry JSON format (ESP32 → server)

```json
{
  "speed_mph": 12.34,
  "delta_us": 628000,
  "squeak_detected": false,
  "squeak_confidence": 0.03,
  "normal_confidence": 0.97,
  "gps_valid": true,
  "gps_hours": 14,
  "gps_minutes": 32,
  "gps_seconds": 8,
  "gps_latitude": 51.501476,
  "gps_longitude": -0.140634,
  "gps_satellites": 9,
  "wheel_circumference_m": 2.18
}
```

## Config JSON format (server → ESP32)

```json
{ "type": "config", "wheel_circumference_m": 2.11 }
```

## REST endpoints

| Method | Path       | Description                        |
|--------|------------|------------------------------------|
| GET    | /          | Dashboard HTML                     |
| GET    | /config    | Current config (ESP32 polls this)  |
| POST   | /config    | Update config from curl/Postman    |
| GET    | /state     | Latest telemetry snapshot          |
| GET    | /history   | Last 300 speed samples as JSON     |

### Example: update circumference via curl

```bash
curl -X POST http://localhost:8000/config \
     -H "Content-Type: application/json" \
     -d '{"wheel_circumference_m": 2.11}'
```