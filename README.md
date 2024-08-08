# ESPHome Ecodan heatpump
It was based on https://github.com/rbroker/ecodan-ha-local. I've also managed to reverse engineer quite some additional properties and controls.
- 0x03 : error codes
- 0x05 : heat source
- 0x0B : refrigerant liquid temperature
- 0x10 : In1, In5, In6 thermostat status
- 0x14 : booster / immersion heater states
- 0x15 : pump status
- 0x28 : forced dhw status 
- 0x35 : room temp setpoint (signed) with flags
- 0xC9 : configuration command. It reports back controller version and much more, need more investigation.

# available languages
English (default), Dutch, Italian, French. Select the language in `ecodan-esphome.yaml` file. 
If you want to contribute with a translation: copy the file `ecodan-labels-en.yaml` to `ecodan-labels-xx.yaml`, fill in all the labels and submit a pull request.

# server control + prohibit dhw/heating/cooling enabled by default
In sever control mode, the prohibit flags can be set. You can disable it by commenting the `sever-control.yaml` entry in the `ecodan-esphome.yaml`.

# recommended hardware
If you don't want to solder, use one of the tested boards. [More boards that should be working](https://github.com/SwiCago/HeatPump/issues/13#issuecomment-457897457). It also should work for airco units with cn105 connectors. 

Tested boards

| Board | Link | Notes |
|:---|:----:|:---|
| m5stack Atom (ESP32 variants) | https://docs.m5stack.com/en/core/ATOM%20Lite | Grove ports used |
| m5stack AtomS3 (ESP32-S3 variants) | https://docs.m5stack.com/en/core/AtomS3%20Lite | Grove ports used |

Cable
* Get one of the grove female cable and a ST PAP-05V-S connector. Remove one end of the grove connector and replace it with a ST PAP-05V-S connector. Here's an example:
![image](https://github.com/gekkekoe/esphome-ecodan-hp/blob/main/img/m5stack_cn105.jpg?raw=true)

Pin mapping (from left to right)
| grove | cn105 |
|:---|:---|
|pin 1 - black (gnd) | pin 4 - black (gnd) |
|pin 2 - red (5v) | pin 3 - red (5v) |
|pin 3 - white (GPIO 2) | pin 2 - white (Tx) |
|pin 4 - yellow (GPIO 1) | pin 1 - yellow (Rx) |

*Note: pin 5 (12v) on the cn105 is not used.*

# where to buy
atom s3 lite
https://www.digikey.nl/en/products/detail/m5stack-technology-co-ltd/C124/18070571

grove cable (multiple options/lengths available)
https://www.digikey.nl/en/products/detail/seeed-technology-co-ltd/110990036/5482563

JST PAP-05V-S connector
https://www.digikey.nl/en/products/detail/jst-sales-america-inc/PAP-05V-S/759977


# build esphome-ecodan-hp firmware
### Build via cmd line:
* Install ESPHome https://esphome.io/guides/getting_started_command_line.html
    ```console
    python3 -m venv venv
    source venv/bin/activate
    pip3 install wheel
    pip3 install esphome
    ```
* Fill in `secrets.yaml` and copy the `ecodan-esphome.yaml` to your esphome folder and edit the values (*check GPO pins (uart: section), you might need to swap the pins in the config*)
The secrets.yaml should at least contain the following entries:
```
wifi_ssid: "wifi network id"
wifi_password: "wifi password"
```
* Edit the following section in `ecodan-esphome.yaml` to match your configuration (esp board, zone1/zone2, language, server control, enable debug). Default is an esp32-s3 board, 1 zone and english language.

```
packages:
  remote_package:
    url: https://github.com/gekkekoe/esphome-ecodan-hp/
    ref: main
    refresh: always
    files: [ 
            confs/esp32s3.yaml, # confs/esp32.yaml, for regular board
            confs/zone1.yaml,
            ## enable if you want to use zone 2
            #confs/zone2.yaml,
            ## enable label language file
            confs/ecodan-labels-en.yaml,
            #confs/ecodan-labels-nl.yaml,
            #confs/ecodan-labels-it.yaml,
            #confs/ecodan-labels-fr.yaml,
            confs/server-control.yaml,
            #confs/debug.yaml,
           ]
```

* Optional: configure cooling/heating action in `ecodan-esphome.yaml`

```
####################
# heatpump heating/cooling switch
# available modes: HEAT_ROOM_TEMP, HEAT_FLOW_TEMP, HEAT_COMPENSATION_CURVE, COOL_ROOM_TEMP, COOL_FLOW_TEMP
substitutions:
  default_heating_switch_mode: HEAT_COMPENSATION_CURVE
  default_cooling_switch_mode: COOL_FLOW_TEMP
```

* Build
```console
esphome compile ecodan-esphome.yaml
```
* To find the tty* where the esp32 is connected at, use `sudo dmesg | grep tty`. On my machine it was `ttyACM0` for usb-c, and `ttyUSB0` for usb-a.
* Connect your esp32 via usb and flash
```console 
esphome upload --device=/dev/ttyACM0 ecodan-esphome.yaml
```
* You can update the firmware via de web interface of the esp after the initial flash, or use the following command to flash over the network
```console 
esphome upload --device ip_address ecodan-esphome.yaml
```

Here's how it's connected inside my heatpump:

![image](https://github.com/gekkekoe/esphome-ecodan-hp/blob/main/img/m5stack_installed.jpg?raw=true)

The esphome component will be auto detected in Home Assistant:

![image](https://github.com/gekkekoe/esphome-ecodan-hp/blob/main/img/ha-integration.png?raw=true)


[!["Buy Me A Coffee"](https://www.buymeacoffee.com/assets/img/custom_images/orange_img.png)](https://www.buymeacoffee.com/gekkekoe)
