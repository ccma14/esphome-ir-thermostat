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

---

## Available Substitutions

| Substitution | Default | Description |
| :--- | :--- | :--- |
| `name` | *(required)* | ESPHome node name / hostname |
| `friendly_name` | *(required)* | Friendly display name in Home Assistant |
| `temperature_sensor` | *(required)* | Home Assistant temperature sensor entity ID |
| `follow_me_interval` | `"10s"` | How often the compensated Follow Me temperature is transmitted |
| `board` | `esp01_1m` | ESP8266 board type (e.g., `esp01_1m`, `d1_mini`, `nodemcuv2`) |
| `ir_tx_pin` | `GPIO4` | GPIO pin connected to the IR LED transmitter |
| `ir_rx_pin` | `GPIO5` | GPIO pin connected to the IR receiver |
| `ir_rx_inverted` | `"true"` | Whether the IR receiver input is active-low |
| `use_fahrenheit` | `"true"` | `"true"` for Fahrenheit (`°F`), `"false"` for Celsius (`°C`) |

---

## Adapting to Other Mini-Split Brands or Transports

The compensation math (`get_compensated_temp()`), room profiles, and Home Assistant tuning entities (`Compensation Speedup`, `Room Size Profile`, `K Filtered`, `Adjusted Temperature`) are completely decoupled from the Midea IR protocol.

Only three sections in [`common/ir_thermostat.yaml`](common/ir_thermostat.yaml) are Midea-specific:

1. **The `climate:` platform** (`platform: midea_ir`, `id: thermostat_ir`):
   ESPHome provides built-in `climate_ir` platforms for many brands (`coolix`, `daikin`, `fujitsu_general`, `gree`, `hitachi_ac344`, `lg`, `mitsubishi`, `panasonic`, `samsung`, `toshiba`, `whirlpool`, etc.). All inherit from ESPHome's base `Climate` class, so `id(thermostat_ir).target_temperature` works identically across every brand.
2. **The `send_follow_me_temp` script** (`parameters: { adjusted_temp_f: float }`):
   Called every `follow_me_interval` with the compensated temperature in `°F`.
3. **The optional Coolix helper codes** (`Mini-Split LED Toggle` and `Follow Me` ON/OFF switch):
   For non-Midea/Coolix units, the `Follow Me` switch acts as a software enable/disable toggle for the periodic compensation loop.

---

### Case A: Another IR Mini-Split with "Follow Me" / "I Feel" (e.g., Gree, Whirlpool, Electrolux)

Many brands (such as Gree's **"I Feel"** or Whirlpool's **"6th Sense"**) periodically transmit an IR packet containing the room temperature measured at the remote.

1. **Capture the IR frame**: Point your original remote (with "I Feel" / "Follow Me" enabled) at the ESP's IR receiver (`ir_rx_pin`). Because `remote_receiver:` is configured with `dump: all`, ESPHome's logs will print the protocol and byte payload.
2. **Override `climate:` and `send_follow_me_temp` in your device YAML** using ESPHome's `!remove` and `!extend` tags (no fork required):

```yaml
substitutions:
  name: ir-living-room
  friendly_name: Living Room Mini-Split
  temperature_sensor: sensor.living_room_temperature

packages:
  thermostat:
    url: https://github.com/ccma14/esphome-ir-thermostat
    ref: main
    files: [common/ir_thermostat.yaml]

# 1. Replace midea_ir with your mini-split's ESPHome climate_ir platform
climate:
  - id: !remove thermostat_ir
  - platform: gree
    id: thermostat_ir
    name: ${friendly_name}
    model: generic
    sensor: ha_temperature_sensor

# 2. Override the Follow Me transmission script for your protocol
script:
  - id: !extend send_follow_me_temp
    then:
      - remote_transmitter.transmit_pronto:  # or transmit_raw / transmit_nec / etc.
          data: !lambda |-
            int temp_c = (int)roundf(f_to_c(adjusted_temp_f));
            return my_custom_follow_me_pronto(temp_c);
```

---

### Case B: Midea / Carrier / Pioneer / MrCool Using a UART Dongle (`midea` component)

If your Midea-compatible unit uses a serial UART dongle instead of an IR blaster, ESPHome's `midea` component includes a native `midea_ac.follow_me` action. Override `send_follow_me_temp` to forward the compensated temperature over UART:

```yaml
script:
  - id: !extend send_follow_me_temp
    then:
      - midea_ac.follow_me:
          temperature: !lambda |-
            return f_to_c(adjusted_temp_f);  // midea_ac.follow_me expects Celsius
          beeper: false
```

---

### Case C: Mini-Splits Without a "Follow Me" Feature (Daikin, Mitsubishi, Fujitsu, LG)

For mini-splits that always read their internal return-air thermistor and do not support a remote temperature beacon, you can still use `get_compensated_temp()` by **applying the inverse offset to the mini-split's target setpoint**:

- **Follow Me mode (Midea)**: When the room is **68°F** and the target is **70°F**, `get_compensated_temp()` reports a lower "fake" room temperature (e.g., **66°F**) so the unit heats harder.
- **Setpoint Offset mode (non-Follow-Me units)**: Instead of reporting a fake room temperature, shift the commanded setpoint sent to the mini-split by the inverse offset ($\text{Target} - (\text{Adjusted} - \text{Actual}) = \mathbf{72^\circ\text{F}}$) whenever the rounded integer setpoint changes by $\ge 1^\circ$ (and increase `follow_me_interval` to e.g. `"300s"` so the indoor unit doesn't beep on frequent updates).


