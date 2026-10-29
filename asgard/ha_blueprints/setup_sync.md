# Home Assistant sync configuration

## 1. Sync Room Temperature
This blueprint links an external temperature reading to the Ecodan Virtual Thermostat. You can choose between the standard version (for climate entities) or the sensor-only version (for standalone temperature sensors).

### Option A: Thermostat Version (Climate Entities)
Use this version if your source is a thermostat or climate entity.

[![Open your Home Assistant instance and show the blueprint import dialog with a specific blueprint URL.](https://my.home-assistant.io/badges/blueprint_import.svg)](https://my.home-assistant.io/redirect/blueprint_import/?blueprint_url=https%3A%2F%2Fgithub.com%2Fgekkekoe%2Fesphome-ecodan-hp%2Fraw%2Fmain%2Fasgard%2Fha_blueprints%2Fsync_virtual_thermostat.yaml)

### Option B: Sensor Version (Any Sensor Entity)
Use this version if your source is a standalone temperature sensor (e.g., a basic Zigbee room sensor).

[![Open your Home Assistant instance and show the blueprint import dialog with a specific blueprint URL.](https://my.home-assistant.io/badges/blueprint_import.svg)](https://my.home-assistant.io/redirect/blueprint_import/?blueprint_url=https%3A%2F%2Fgithub.com%2Fgekkekoe%2Fesphome-ecodan-hp%2Fraw%2Fmain%2Fasgard%2Fha_blueprints%2Fsync_virtual_thermostat_sensor.yaml)

### What the sync refuses, and how it starts again

**No zero.** The number accepts −10..40 °C, so its own range would let a `0`
through, and `0` is what a sensor publishes after it drops off the network. The
blueprint's **Lowest reading to forward** (default 0.1 °C) is the guard; an
`unavailable` source or target is dropped as well.

**It starts again.** The sync also runs when the ESPHome device reports a value of
its own (how a reboot shows up), when Home Assistant starts, and every five
minutes. A value that already matches is never re-sent, so the re-check costs
nothing while both sides agree.

---

### How does this work?
1. Ensure your Home Assistant instance is running.
2. Click the blue button above for the version you need.
3. Home Assistant will automatically open the import dialog.
4. You will see a preview of the blueprint and can click **"Import Blueprint"**.

---

## 2. Sync Battery or Room Temperature to ODIN (REST)
This blueprint pushes a Home Assistant sensor value straight into an ODIN on the
LAN over its REST API — no MQTT needed. Pick the field it feeds:

| ODIN field | Source sensor | Accepted values |
| --- | --- | --- |
| `battery_soc_kwh` | battery energy, kWh | 0..2000 |
| `battery_max_discharge_kw` | power, kW | 0..100 |
| `room_override_z1` | room temperature, °C | 5..40 |
| `room_override_z2` | room temperature, °C | 5..40 |

Create **one automation per field** — four automations if you feed all of them.
The **Room Temperature Override** is for an ODIN that has no room sensor of its
own, or one that reads the boiler room instead of the living room: the number
replaces the measured room temperature for **planning only**, so the history and
the learned heat model keep the real sensor, and the dashboard shows which of the
two is in use. Below 5 °C — `0` included — nothing is forwarded: that range is the
difference between "override off" and a heated room, so an offline sensor leaves
the ODIN on the value it had, and clearing the override stays a dashboard action.
Writes only happen on a difference, repeated every five minutes, so a reboot or a
dropout does not end the sync and the ODIN keeps trusting the number as a
measurement.

[![Open your Home Assistant instance and show the blueprint import dialog with a specific blueprint URL.](https://my.home-assistant.io/badges/blueprint_import.svg)](https://my.home-assistant.io/redirect/blueprint_import/?blueprint_url=https%3A%2F%2Fgithub.com%2Fgekkekoe%2Fesphome-ecodan-hp%2Fraw%2Fmain%2Fasgard%2Fha_blueprints%2Fsync_odin.yaml)

---

## 3. Manual Setup
If you prefer to set this up manually instead of using a blueprint, you can use the instructions below. 

### Instructions
1. Go to **Home Assistant** > **Settings** > **Automations & Scenes**.
2. Click **Create Automation** > **Create new automation**.
3. Click the options menu (three dots) in the top right corner and select "Edit in YAML." Paste the configuration below, replacing the sensor entity ID and the number entity ID everywhere they appear.

```yaml
alias: "Sync Room Temperature to Ecodan (Manual Sensor Version)"
description: >
  Syncs a temperature sensor to the Virtual Thermostat: a zero is never forwarded,
  and the sync resumes by itself after a reboot or a dropout.

trigger:
  - platform: state
    entity_id: sensor.room_temp_sensor
  # The device reporting its own value is how a reboot or a dropout shows up.
  - platform: state
    entity_id: number.ecodan_heatpump_virtual_thermostat_input_z1
    not_from: [unknown, unavailable]
  - platform: event
    event_type: homeassistant_start
  # "/5" is every five minutes; a bare 5 would fire once an hour.
  - platform: time_pattern
    minutes: "/5"

variables:
  room: "{{ states('sensor.room_temp_sensor') | float(-999) }}"
  # On the number's own 0.1 step: a finer reading would never match and rewrite forever.
  send: "{{ (room * 10) | round(0) / 10 }}"
  held: "{{ states('number.ecodan_heatpump_virtual_thermostat_input_z1') | float(-999) }}"

condition:
  # The number accepts -10 °C, so this floor keeps a dead sensor's 0 out.
  - condition: template
    value_template: "{{ send > 0.1 }}"
  - condition: template
    value_template: >
      {{ states('number.ecodan_heatpump_virtual_thermostat_input_z1') not in ('unknown', 'unavailable') }}
  # Single precision on the device: equality would re-send the same reading.
  - condition: template
    value_template: "{{ (send - held) | abs > 0.05 }}"

action:
  - service: number.set_value
    target:
      entity_id: number.ecodan_heatpump_virtual_thermostat_input_z1
    data:
      value: "{{ send }}"
    continue_on_error: true
  - delay: "00:01:00"

mode: queued
max_exceeded: silent
```