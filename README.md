# AI-For-Science-2026

##[Webserver](https://paris-connor.github.io/AI-For-Science-2026/)
### Demos
.[ChatGPTDrum.html](https://paris-connor.github.io/AI-For-Science-2026/ChatGPTDrum.html)
.[GeminiDrums.html](https://paris-connor.github.io/AI-For-Science-2026/GeminiDrums.html)
.[GrokDrum.html](https://paris-connor.github.io/AI-For-Science-2026/GrokDrum.html)
.[ClaudeDrum.html](https://paris-connor.github.io/AI-For-Science-2026/ClaudeDrum.html)

---

# AG Tech - ESP8266 Environmental Monitor

Environmental monitoring system using a Wemos D1 Mini (ESP8266) and DHT11 sensor with a live web dashboard.

## Hardware

| Component | Details |
|-----------|---------|
| Board | Wemos D1 Mini (ESP8266EX, 4MB flash) |
| Sensor | DHT11 (temperature & humidity) |
| USB Chip | FTDI (serial port: `/dev/cu.usbserial-A50285BI`) |
| MAC | `f4:cf:a2:e4:3a:4d` |

## Wiring

```
DHT11          D1 Mini
─────          ───────
VCC  ────────  3.3V
DATA ────────  D5 (GPIO14)
GND  ────────  GND
```

If using a raw 4-pin DHT11 (not a module), add a 10k ohm pull-up resistor between DATA and VCC.

## Project Structure

```
AG_Tech/
├── led_blink/          # Step 1: LED blink test sketch
│   └── led_blink.ino
├── dht11_test/         # Step 2: DHT11 serial output test
│   └── dht11_test.ino
├── dashboard/          # Step 3: Full web dashboard
│   ├── dashboard.ino
│   ├── config.h        # Your WiFi credentials (git-ignored)
│   └── config.example.h
├── sensor_scan/        # Utility: detect connected sensors
│   └── sensor_scan.ino
├── docs/
│   ├── SETUP.md        # Installation & setup guide
│   ├── WIRING.md       # Pin reference & wiring diagrams
│   └── API.md          # Dashboard JSON API reference
├── .gitignore
└── README.md
```

## Quick Start

### 1. Install arduino-cli

```bash
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=./bin sh
export PATH="./bin:$PATH"
```

### 2. Install ESP8266 board support

```bash
arduino-cli config init
arduino-cli config add board_manager.additional_urls https://arduino.esp8266.com/stable/package_esp8266com_index.json
arduino-cli core update-index
arduino-cli core install esp8266:esp8266
```

### 3. Install libraries

```bash
arduino-cli lib install "DHT sensor library" "Adafruit Unified Sensor"
```

### 4. Configure WiFi

```bash
cp dashboard/config.example.h dashboard/config.h
# Edit dashboard/config.h with your WiFi SSID and password
```

### 5. Compile and upload

```bash
arduino-cli compile --fqbn esp8266:esp8266:d1_mini dashboard
arduino-cli upload --fqbn esp8266:esp8266:d1_mini --port /dev/cu.usbserial-A50285BI dashboard
```

### 6. Open the dashboard

Check serial output for the IP address (115200 baud), then open `http://<IP>` in a browser.

## Dashboard Features

- **Live temperature & humidity cards** - updates every 2 seconds
- **Real-time line graphs** - scrolling history of last ~4 minutes (120 readings)
- **Event log** - timestamped reading log in the browser
- **Serial output** - all readings also printed at 115200 baud
- **JSON API** - `GET /data` returns current + historical readings

## Apple Silicon Note

The ESP8266 toolchain requires Rosetta 2 on Apple Silicon Macs:

```bash
softwareupdate --install-rosetta --agree-to-license
```

## License

MIT
