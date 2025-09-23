# ESPHome Ecodan heatpump
ESPHome implementation of the CN105 protocol. It can operate as standalone or with slave (Melcloud/Procon) attached. It includes server control mode, realtime power output estimation, and realtime daily energy counters.

The remote thermostat protocol CNRF is supported by [esphome-ecodan-remote-thermostat](https://github.com/gekkekoe/esphome-ecodan-remote-thermostat). It implements a virtual thermostat that can be linked with any temperature sensor.

# Available Languages
Select the language in the `ecodan-esphome.yaml` file.  

| Language | ISO Code |
|----------|----------|
| English | en |
| Dutch | nl |
| Italian | it |
| French | fr |
| Spanish | es |
| German | de |
| Finnish | fi |
| Norwegian | no |
| Swedish | sv |
| Danish | da |
| Polish | pl |

If you want to contribute a translation: copy the file `ecodan-labels-en.yaml` to `ecodan-labels-xx.yaml` (replace `xx` with your language code), fill in all the labels, and submit a pull request.

# Features

| Feature  | Description  |
|----------|--------------|
| Monitor | Monitor flow, frequency, temperatures, and more |
| Control | Operating mode, target temperature, flow temperature, on/standby, vacation, and more |
| Server Control | Restrict/Allow DHW/Heating/Cooling |
| Proxy support | Use a Melcloud WiFi adapter or Procon Melcobems unit in combination with the ESP |
| Realtime metrics | `Power output`, `Consumption`, `Production`, `COP` (no external meter required, FTC6+) |
| Misc* | `Fan speed`, `Compressor starts`, `Outdoor unit thermistors` |
| Updates | OTA updates available when using the default recommended hardware |
| Short cycle detection & mitigation | Protects your heat pump from excessive on/off cycling. [Documentation](https://github.com/gekkekoe/esphome-ecodan-hp/blob/main/docs/short-cycle-mitigation.md) |
| Auto Adaptive Control | Automatically adjust flow temps to maintain room temp. (Self learning and fine tuning) [Documentation](https://github.com/gekkekoe/esphome-ecodan-hp/blob/main/docs/auto-adaptive.md) |


\* <sup><i>These features are not available in proxy, since we are observing only the ESP metrics</i></sup>

# Links
* [Recommended hardware](https://github.com/gekkekoe/esphome-ecodan-hp/blob/main/docs/hardware.md)
* [Recommended hardware for proxy setup](https://github.com/gekkekoe/esphome-ecodan-hp/blob/main/docs/proxy.md)
* [Install from prebuilt binaries](https://github.com/gekkekoe/esphome-ecodan-hp/blob/main/docs/install-from-bin.md)
* [Build from source](https://github.com/gekkekoe/esphome-ecodan-hp/blob/main/docs/build-from-source.md)

# Result
Here's how it's connected inside the heatpump:

![image](https://github.com/gekkekoe/esphome-ecodan-hp/blob/main/img/m5stack_installed.jpg?raw=true)

The ESPHome component will be auto detected in Home Assistant:

![image](https://github.com/gekkekoe/esphome-ecodan-hp/blob/main/img/ha-integration.png?raw=true)

[!["Buy Me A Coffee"](https://www.buymeacoffee.com/assets/img/custom_images/orange_img.png)](https://www.buymeacoffee.com/gekkekoe)
