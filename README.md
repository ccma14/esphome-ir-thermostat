# ESPHome Midea IR Thermostat Controller ("Follow Me" Compensation)

An ESPHome package for controlling Midea/Coolix-compatible mini-split heat pumps via infrared (IR) using an external Home Assistant temperature sensor and adaptive **Follow Me** temperature compensation.

## Features

- **Adaptive Follow Me Compensation (`common/ir_thermostat.h`)**:
  - Dynamically adjusts the reported "Follow Me" temperature based on the delta between actual room temperature and target setpoint.
  - Configurable **Room Size Profile** (`Small`, `Medium`, `Large`) that tunes gain smoothing time constant ($\tau$), maximum temperature offset ("hide" limit), and slew-rate limiting.
  - Configurable **Compensation Speedup** (`-3` to `+3`) exposed as a Home Assistant number entity.
  - Publishes internal filtered gain (`K Filtered`) and compensated temperature (`Adjusted Temperature`) back to Home Assistant sensors.
- **Zero Hardcoded Credentials**: All Wi-Fi, OTA, Fallback Hotspot, and API credentials are loaded via ESPHome's `!secret` mechanism from `secrets.yaml`.

---

## Repository Structure

```text
├── common/
│   ├── ir_thermostat.yaml   # Reusable ESPHome package definition
│   └── ir_thermostat.h      # Follow Me smoothing & IR payload encoding
├── components/
│   └── ir_thermostat/
│       └── __init__.py      # External component loader that bundles ir_thermostat.h
├── ir_example.yaml          # Example per-room device configuration
├── secrets.yaml.example     # Template for local secrets.yaml
└── .gitignore
```

---

## Setup & Configuration

### 1. Configure `secrets.yaml`

Copy the example secrets file and populate it with your credentials:

```bash
cp secrets.yaml.example secrets.yaml
```

Edit `secrets.yaml`:

```yaml
wifi_ssid: "your_wifi_ssid"
wifi_password: "your_wifi_password"
wifi_domain: ".local"
ota_password: "your_ota_password"
ap_password: "your_fallback_ap_password"
api_encryption_key: "your_base64_api_encryption_key"
```

> `secrets.yaml` is listed in `.gitignore` and will never be committed to git.

### 2. Option A: Use Directly from a Local Clone

Copy `ir_example.yaml` for your room (e.g., `ir_bedroom.yaml`) and set the substitutions for your device and Home Assistant temperature sensor:

```yaml
substitutions:
  name: ir-bedroom
  friendly_name: Bedroom IR
  temperature_sensor: sensor.bedroom_temperature
  # Optional: override the Follow Me transmission interval (default: "10s")
  # follow_me_interval: "10s"

packages:
  common: !include common/ir_thermostat.yaml
```

Compile and flash with ESPHome:

```bash
esphome run ir_bedroom.yaml
```

### 3. Option B: Consume as a Remote Git Package

You can include this repository directly from your own private ESPHome configuration directory without cloning it manually:

```yaml
substitutions:
  name: ir02
  friendly_name: Bedroom IR
  temperature_sensor: sensor.bedroom_temperature_calibrated

packages:
  thermostat:
    url: https://github.com/ccma14/esphome-ir-thermostat
    ref: main
    files: [common/ir_thermostat.yaml]
    refresh: 1d
```

