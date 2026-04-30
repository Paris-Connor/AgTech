# AgTech - Smart Plant Monitoring System

Real-time environmental monitoring for plants using a Wemos D1 Mini (ESP8266) with multiple sensors, RGB status LED, and a live web dashboard.

**Live Dashboard:** [paris-connor.github.io/AgTech](https://paris-connor.github.io/AgTech/)

## Features

- **4 sensors** — temperature, humidity, light intensity, soil moisture
- **RGB LED status indicator** — green (ok), yellow (warning), red (danger)
- **Live web dashboard** served from the D1 Mini with real-time charts
- **Cloud dashboard** via ThingSpeak — view from anywhere
- **GitHub Pages dashboard** with demo mode, local connect, and cloud modes
- **Auto-detection** — sensors that aren't connected are gracefully skipped
- **Configurable thresholds** for all sensors

## Hardware

| Component | Pin | Details |
|-----------|-----|---------|
| DHT11 (temp/humidity) | D5 (GPIO14) | 3.3V, data pin |
| LM393 light sensor | D2 (GPIO4) | Digital out (bright/dark), 3.3V |
| Soil Moisture Sensor | A0 | Analog, 3.3V |
| RGB LED - Red | D6 (GPIO12) | 220 ohm resistor |
| RGB LED - Green | D7 (GPIO13) | 220 ohm resistor |
| RGB LED - Blue | D8 (GPIO15) | 220 ohm resistor |
| RGB LED - GND | GND | Common cathode (long leg) |

## Thresholds (Fittonia profile)

| Sensor | OK (Green) | Warning (Yellow) | Danger (Red) |
|--------|-----------|-----------------|-------------|
| Temperature | 65-75 F | 55-65 / 75-85 F | <55 / >85 F |
| Humidity | 45-65% | 25-45 / 65-75% | <25 / >75% |
| Light | 250-1000 | <250 / >1000 | <100 / >1500 |
| Soil Moisture | 40-75% | 25-40 / 75-85% | <25 / >85% |

Other plant profiles (Pothos, Snake, Succulent, Basil, Tomato) are selectable from the dashboard dropdown.

## Quick Start

```bash
# Install tools
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=./bin sh
export PATH="./bin:$PATH"

# Install board + libraries
arduino-cli config init
arduino-cli config add board_manager.additional_urls https://arduino.esp8266.com/stable/package_esp8266com_index.json
arduino-cli core update-index && arduino-cli core install esp8266:esp8266
arduino-cli lib install "DHT sensor library" "Adafruit Unified Sensor" "BH1750" "WiFiManager"

# Configure (ThingSpeak only - WiFi is set later via captive portal)
cp dashboard/config.example.h dashboard/config.h
# Optional: edit config.h to add ThingSpeak Write API key for cloud push

# Flash
arduino-cli compile --fqbn esp8266:esp8266:d1_mini dashboard
arduino-cli upload --fqbn esp8266:esp8266:d1_mini --port /dev/cu.usbserial-XXXXXXXX dashboard
```

## First-Time WiFi Setup (no computer needed)

After flashing, the D1 Mini has no saved WiFi yet. To get it online:

1. **Plug it in** to any USB power source. The RGB LED turns **magenta/purple** (setup mode).
2. **On your phone**, open WiFi settings and connect to **`AG-Tech-Setup`** (no password).
3. A captive portal opens automatically. If it doesn't, open a browser and visit `192.168.4.1`.
4. Tap **Configure WiFi**, pick your network from the list, type the password, and Save.
5. The D1 Mini reboots, joins your network, and the LED flashes green. The dashboard is live at the IP your router assigned (visible in serial output, or check your router's client list).

Credentials are saved to flash - it will rejoin automatically on every future power-up.

## Moving to a New WiFi Network

The dashboard portal is the same setup procedure as first-time:

1. Move the device to the new location and plug into power.
2. It tries the saved network. If it can't find it, the LED turns magenta and `AG-Tech-Setup` reappears.
3. Connect your phone, configure the new network as above.

No reflash, no laptop, no cable required. The same physical device works on any 2.4 GHz WiFi.

## LED Status Reference

| Color | Meaning |
|-------|---------|
| Magenta / Purple | Needs WiFi setup - connect phone to `AG-Tech-Setup` |
| Blue (solid) | Connecting to saved WiFi |
| Green flash | Just connected successfully |
| Green | All sensors OK |
| Yellow | Warning - one or more readings out of ideal range |
| Red | Danger - reading is in critical range, or DHT11 read failed |

## Project Structure

```
AgTech/
├── dashboard/            # Main firmware (all sensors + web UI + ThingSpeak)
│   ├── dashboard.ino
│   ├── config.h          # WiFi + API keys (git-ignored)
│   └── config.example.h
├── index.html            # GitHub Pages dashboard (demo/local/cloud modes)
├── led_blink/            # LED blink test
├── dht11_test/           # DHT11 serial test
├── rgb_test/             # RGB LED color test
├── sensor_scan/          # Auto-detect connected sensors
├── i2c_scan/             # I2C bus scanner
├── docs/
│   ├── SETUP.md          # Full setup guide
│   ├── WIRING.md         # Pin diagrams
│   └── API.md            # JSON API + ThingSpeak reference
└── README.md
```

## Dashboard Modes

The GitHub Pages dashboard supports three data sources:

| Mode | How | When |
|------|-----|------|
| **Demo** | Simulated data | Default, always works |
| **Local** | Enter D1 Mini IP | Same WiFi network |
| **Cloud** | Enter ThingSpeak Channel ID + Read Key | Anywhere with internet |

## Apple Silicon Note

```bash
softwareupdate --install-rosetta --agree-to-license
```
