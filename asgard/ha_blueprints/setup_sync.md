# Temperature sync configuration

## 1. Sync Room Temperature
This blueprint links an external temperature reading to the Ecodan Virtual Thermostat. You can choose between the standard version (for climate entities) or the sensor-only version (for standalone temperature sensors).

### Option A: Thermostat Version (Climate Entities)
Use this version if your source is a thermostat or climate entity.

[![Open your Home Assistant instance and show the blueprint import dialog with a specific blueprint URL.](https://my.home-assistant.io/badges/blueprint_import.svg)](https://my.home-assistant.io/redirect/blueprint_import/?blueprint_url=https%3A%2F%2Fgithub.com%2Fgekkekoe%2Fesphome-ecodan-hp%2Fraw%2Fmain%2Fasgard%2Fha_blueprints%2Fsync_virtual_thermostat.yaml)

### Option B: Sensor Version (Any Sensor Entity)
Use this version if your source is a standalone temperature sensor (e.g., a basic Zigbee room sensor).

[![Open your Home Assistant instance and show the blueprint import dialog with a specific blueprint URL.](https://my.home-assistant.io/badges/blueprint_import.svg)](https://my.home-assistant.io/redirect/blueprint_import/?blueprint_url=https%3A%2F%2Fgithub.com%2Fgekkekoe%2Fesphome-ecodan-hp%2Fraw%2Fmain%2Fasgard%2Fha_blueprints%2Fsync_virtual_thermostat_sensor.yaml)

---

### How does this work?
1. Ensure your Home Assistant instance is running.
2. Click the blue button above for the version you need.
3. Home Assistant will automatically open the import dialog.
4. You will see a preview of the blueprint and can click **"Import Blueprint"**.

---

## 2. Manual Setup
If you prefer to set this up manually instead of using a blueprint, you can use the instructions below. 

### Instructions
1. Go to **Home Assistant** > **Settings** > **Automations & Scenes**.
2. Click **Create Automation** > **Create new automation**.
3. Click the options menu (three dots) in the top right corner and select "Edit in YAML." Paste the YAML configuration provided below, ensuring you replace all three instances of `room_temp_sensor` with your actual sensor's entity ID

```yaml
alias: "Sync Room Temperature to Ecodan (Manual Sensor Version)"
description: "Synchronizes a standalone temperature sensor to the Ecodan Virtual Thermostat."
mode: single

trigger:
  - platform: state
    # Replace this with your actual temperature sensor entity ID
    entity_id: sensor.room_temp_sensor

condition:
  - condition: template
    value_template: >
      {{ states('sensor.room_temp_sensor') | float(0) != states('number.ecodan_heatpump_virtual_thermostat_input_z1') | float(0) }}

action:
  - service: number.set_value
    target:
      # Replace this with your actual ESPHome target number entity ID
      entity_id: number.ecodan_heatpump_virtual_thermostat_input_z1
    data:
      value: "{{ states('sensor.room_temp_sensor') | float(0) }}"

  - delay: "00:01:00"

```