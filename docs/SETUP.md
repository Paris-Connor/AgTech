# Setup Guide

## Hardware Required

- Wemos D1 Mini (ESP8266)
- DHT11 temperature/humidity sensor (3-pin module)
- LM393 light sensor module (digital output)
- Capacitive soil moisture sensor (analog)
- RGB LED (common cathode) + 3x 220 ohm resistors
- Breadboard + jumper wires
- Micro-USB cable
- USB power source (wall charger, power bank, or any 5V USB port - no computer needed once flashed)

## Software Installation

### 1. Install Rosetta 2 (Apple Silicon Macs only)

```bash
softwareupdate --install-rosetta --agree-to-license
```

### 2. Install arduino-cli

```bash
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=./bin sh
export PATH="./bin:$PATH"
```

### 3. Install ESP8266 board support

```bash
arduino-cli config init
arduino-cli config add board_manager.additional_urls \
  https://arduino.esp8266.com/stable/package_esp8266com_index.json
arduino-cli core update-index
arduino-cli core install esp8266:esp8266
```

### 4. Install libraries

```bash
arduino-cli lib install "DHT sensor library" "Adafruit Unified Sensor" "BH1750" "WiFiManager"
```

### 5. Configure ThingSpeak (optional)

```bash
cp dashboard/config.example.h dashboard/config.h
```

Edit `config.h` to add your ThingSpeak Write API key if you want cloud push.
**WiFi is not configured here** - it's set at first boot via the captive portal.

### 6. Compile and upload

```bash
arduino-cli compile --fqbn esp8266:esp8266:d1_mini dashboard
arduino-cli upload --fqbn esp8266:esp8266:d1_mini --port /dev/cu.usbserial-XXXXXXXX dashboard
```

### 7. Connect to WiFi (no computer needed)

After the upload finishes:

1. The D1 Mini's RGB LED turns **magenta** - this means it needs WiFi setup.
2. On your phone, connect to the WiFi network **`AG-Tech-Setup`** (no password).
3. A captive portal page opens automatically. If it doesn't, browse to `192.168.4.1`.
4. Tap **Configure WiFi** -> choose your network -> enter password -> Save.
5. The D1 Mini reboots and connects. LED flashes green when online.

### 8. Find the dashboard

The IP address will be printed on serial (115200 baud) at boot, or check your router's connected-clients list. The same IP serves the dashboard - just open it in any browser on the same network.

## ThingSpeak Setup

1. Create a free account at https://thingspeak.mathworks.com
2. Create a new channel with 4 fields: Temperature, Humidity, Light, Soil Moisture
3. Copy the **Write API Key** into `dashboard/config.h`
4. Copy the **Channel ID** and **Read API Key** for the GitHub Pages dashboard
5. Open https://paris-connor.github.io/AgTech/ and enter the Channel ID + Read API Key

## Running Without a Computer

Once flashed, the D1 Mini doesn't need a computer. Plug it into any USB power source:

- USB wall charger (best for permanent install)
- USB power bank (portable - lasts a few days)
- Any 5V USB port

It will auto-connect to its saved WiFi and start serving the dashboard. To check on it remotely:

- Local: open `http://<d1-ip>` in a browser on the same network
- Cloud: ThingSpeak channel (if configured)
- GitHub Pages: `https://paris-connor.github.io/AgTech/`

If the LED is magenta, it can't reach the saved network - configure via the captive portal as in step 7.

## Soil Moisture Calibration

The soil sensor outputs an analog voltage that maps to moisture percentage. Default calibration:

- `SOIL_DRY = 1023` (raw value in air = 0% moisture)
- `SOIL_WET = 300` (raw value in water = 100% moisture)

To calibrate for your soil:
1. Read the raw A0 value in dry air (check serial output)
2. Read the raw A0 value submerged in water
3. Update `SOIL_DRY` and `SOIL_WET` in `dashboard.ino`

## Troubleshooting

| Problem | Solution |
|---------|----------|
| `bad CPU type in executable` | Install Rosetta 2 |
| Board not detected | Try a different USB cable |
| `Failed to read from DHT11` | Check wiring: DATA to D5, VCC to 3.3V, GND |
| WiFi won't connect | Use the captive portal (see step 7). D1 only supports 2.4GHz networks |
| LED stays magenta | No saved WiFi - connect phone to `AG-Tech-Setup` and configure |
| Need to switch networks | Just power on at the new location - if old network isn't found, the portal reappears automatically |
| LM393 always reads same value | Adjust the small potentiometer on the sensor module to set the bright/dark threshold |
| Soil reads N/A | Sensor not connected or A0 reads 1023 (open circuit) |
| ThingSpeak fails | Check API key, ensure WiFi connected, 15s minimum interval |
| Serial shows garbage | Ensure baud rate is 115200 |
| Blue LED always on | Don't use D1 (GPIO5) for LED - has built-in pull-up |
