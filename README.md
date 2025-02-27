# ESPHome Ecodan heatpump
ESPHome implementation of the CN105 protocal. It can operate as standalone or with slave (melcloud/procon) attached. It includes server control mode, realtime power output estimation and realtime daily energy counters. 

The remote thermostat protocol CNRF is supported by https://github.com/gekkekoe/esphome-ecodan-remote-thermostat. It implements a virtual thermostat that can be linked with any temperature sensor.

# available languages
English (default), Dutch, Italian, French, Spanish. Select the language in `ecodan-esphome.yaml` file. 
If you want to contribute with a translation: copy the file `ecodan-labels-en.yaml` to `ecodan-labels-xx.yaml`, fill in all the labels and submit a pull request.

# features

| Feature  | description  |
|----------|--------------|
| Server Control | Restrict/Allow Dhw/Heating/Cooling  |
| Proxy support | Use a Melcloud WiFi adapter or procon melcobems unit in combination with the esp  |
| Realtime metrics | `Power output`, `Consumption`, `Production`, `COP` (no external meter required, FTC6+)  |
| Misc |  `Fan speed`, `Compressor starts`, `Outdoor unit thermistors` |

# links
* [Recommended hardware](https://github.com/gekkekoe/esphome-ecodan-hp/blob/main/docs/hardware.md)
* [Recommended hardware for proxy setup](https://github.com/gekkekoe/esphome-ecodan-hp/blob/main/docs/proxy.md)
* [Install from prebuilt binaries](https://github.com/gekkekoe/esphome-ecodan-hp/blob/main/docs/install-from-bin.md)
* [Build from source](https://github.com/gekkekoe/esphome-ecodan-hp/blob/main/docs/build-from-source.md)

# result
Here's how it's connected inside the heatpump:

![image](https://github.com/gekkekoe/esphome-ecodan-hp/blob/main/img/m5stack_installed.jpg?raw=true)

The esphome component will be auto detected in Home Assistant:

![image](https://github.com/gekkekoe/esphome-ecodan-hp/blob/main/img/ha-integration.png?raw=true)


[!["Buy Me A Coffee"](https://www.buymeacoffee.com/assets/img/custom_images/orange_img.png)](https://www.buymeacoffee.com/gekkekoe)
