
## Standalone Auto Adaptive Installation Guide (Asgard only)
The local dashboard can be accessed at http://ecodan-heatpump.local/dashboard (or http://esp_ip/dashboard).

![Standalone config](img/sa-config.png?v=3) 

### Badges
Key performance indicators from your heatpump [1]

### Zone 1 / Zone 2
Settings for Zone 1 and optionally Zone 2. [2, 3, 4, 5]

### Auto Adaptive Settings
Auto Adaptive Settings for zone 1 and zone 2 [6, 7]

### DHW Settings
Configure and control your hot water settings [8]


## Configure Auto Adaptive Mode
1.  Use the `Operating Mode` selection box to select `Heat Flow Temperature` [2] 
2.  Use the `Room Temp Source Z1` selection box to select the room temperature source. `Choose Asgard virtual thermostat` [7]
3.  Use the `Temp Sensor Source` selection box to select the temperature sensor source for the virtual thermostat. [4]
    * Virtual Thermostat input: Use this if you have a sensor that is updating via REST API (see section below)
    * Ds18x20: Use this option if you are using the dallas temperature sensors
    * MRC: Use this option if you don't have any external temperature sensors. The temperature sensor is located within the main display. Please beware of the low resolution (0.5c) of this option.
4.  Enable the Auto Adaptive `Enable AA Control` switch to enable the auto adaptive algorithm [6]
5.  [Optional] For 2 zone systems: Apply Step 1 - 3 

> [!TIP]
> **Greyed thermostats:** When `SW1-8` is in the `OFF` position, the Room thermostats will be disabled [5]. When `SW1-8` is in the `ON` position, the Virtual thermostats will be disabled. 

### Configure virtual thermostat temperature sensor via REST API
The esphome component will expose a REST API for the feedback values. You can use GET/POST to access/modify the current temperature. The entities will be named `asgard_vt_input_z1` for zone 1 and `asgard_vt_input_z2` for zone 2. Any temperature sensor that can communicate over REST API can be used.

### Example Get data z1
```bash
curl -X GET "http://ecodan-heatpump.local/number/Virtual%20Thermostat%20Input%20z1"
   
# returns
{"name_id":"number/Virtual Thermostat Input z1","id":"number-virtual_thermostat_input_z1","value":"22.0","state":"22.0 °C"}%
```

### Example Post data z1 (set 21.5c)
```bash
curl -X POST "http://ecodan-heatpump.local/number/Virtual%20Thermostat%20Input%20z1/set?value=21.5" -d ""
```

> [!TIP]
> **Standalone history:** When running in standalone mode, the (graph) data is stored for 24h on Asgard. Use a browser and zoom/scroll back up to 24h in time. Please note that data is lost on reboot.
