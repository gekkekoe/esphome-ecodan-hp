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