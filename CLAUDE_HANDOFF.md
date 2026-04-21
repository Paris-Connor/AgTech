# AgTech Project Handoff

## What this is
An ESP8266 (Wemos D1 Mini) plant monitoring system with a web dashboard, RGB status LED, and ThingSpeak cloud integration.

## Repo
https://github.com/Paris-Connor/agtech-clone

## Current state - what works
- **DHT11** temp/humidity sensor on D5 (GPIO14) - working
- **RGB LED** status indicator on D6 (red), D7 (green), D8 (blue) - working, common cathode
- **Web dashboard** served from D1 at whatever IP it gets assigned - working
- **ThingSpeak cloud push** every 20 seconds - working (Channel 3352485, Write Key: QAK03A9LQN8HRKAJ)
- **GitHub Pages dashboard** at https://paris-connor.github.io/AgTech/ (Channel ID: 3352485, Read Key: AKAVP06XWNJO345V)
- **Soil moisture sensor** code on A0 - code ready, auto-detects, but no physical probe connected yet

## What needs to be done

### 1. Re-solder and test GY-30 light sensor
The BH1750 light sensor (GY-30 module) has bad solder joints on its header pins. The code already supports it - just needs working hardware.
- Solder the header pins properly (shiny cone-shaped joints)
- Connect: VCC→3.3V, GND→GND, SDA→D2 (GPIO4), SCL→D1 (GPIO5)
- ADDR pin left unconnected
- Flash the dashboard and check serial output for "GY-30 (BH1750) found!"
- If still not found, run the I2C scanner: `i2c_scan/i2c_scan.ino`

### 2. Connect soil moisture probe
- Plug into: VCC→3.3V, GND→GND, SIG→A0
- Auto-detects on boot - no code changes needed
- Calibration values in dashboard.ino: SOIL_DRY=1023, SOIL_WET=300 (adjust after testing)
- Without the probe, A0 floats and reads as 100% soil moisture (false danger alert) - this is expected

## How to build and flash

```bash
# Clone
git clone https://github.com/Paris-Connor/agtech-clone.git
cd agtech-clone

# Install arduino-cli
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=./bin sh
export PATH="./bin:$PATH"

# On Apple Silicon, need Rosetta
softwareupdate --install-rosetta --agree-to-license

# Setup board + libraries
arduino-cli config init
arduino-cli config add board_manager.additional_urls https://arduino.esp8266.com/stable/package_esp8266com_index.json
arduino-cli core update-index
arduino-cli core install esp8266:esp8266
arduino-cli lib install "DHT sensor library" "Adafruit Unified Sensor" "BH1750"

# Create config with WiFi + ThingSpeak
cp dashboard/config.example.h dashboard/config.h
# Edit config.h: WiFi SSID/pass + ThingSpeak Write API Key

# Compile and upload
arduino-cli compile --fqbn esp8266:esp8266:d1_mini dashboard
arduino-cli upload --fqbn esp8266:esp8266:d1_mini --port /dev/cu.usbserial-XXXXXXXX dashboard
```

## Pin map
| Pin | GPIO | Device |
|-----|------|--------|
| D1 | 5 | GY-30 SCL (I2C) |
| D2 | 4 | GY-30 SDA (I2C) |
| D5 | 14 | DHT11 data |
| D6 | 12 | RGB LED Red (220Ω resistor) |
| D7 | 13 | RGB LED Green (220Ω resistor) |
| D8 | 15 | RGB LED Blue (220Ω resistor) |
| A0 | ADC | Soil moisture probe |

## Important notes
- D1 Mini only supports 2.4GHz WiFi
- D1 (GPIO5) has a built-in pull-up - never use it for LEDs
- D8 (GPIO15) has a built-in pull-down - must be LOW at boot (fine for LED, starts off)
- RGB LED is common cathode (long leg to GND)
- ThingSpeak free tier: minimum 15 second update interval (code uses 20s)
- The dashboard HTML is embedded in the .ino file as a raw string literal
- config.h is git-ignored to protect WiFi credentials
- Serial baud rate is 115200

## LED behavior
- Blue blink: connecting to WiFi
- Green: all sensors OK
- Yellow/Orange: warning threshold
- Red: danger threshold or sensor error
