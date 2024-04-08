# ESPHome version of Ecodan ha local
It is highly inspired by https://github.com/rbroker/ecodan-ha-local, https://github.com/tobias-93/esphome-ecodan-heatpump and https://github.com/vekexasia/comfoair-esp32

# ecodan-esp32 example configurations
ESP32(S3) configuration files ecodan-ha-local and esphome

# required hardware
If you don't want to solder, use one of the boards that supports 5v on the GPIO ports. It also should work for airco units with cn105 connectors.

Tested boards

| Board | Link | Notes |
|:---|:----:|:---|
| m5stack Atom (ESP32 variants) | https://docs.m5stack.com/en/core/ATOM%20Lite | Grove ports on the m5stack boards have built in level shifters |
| m5stack AtomS3 (ESP32-S3 variants) | https://docs.m5stack.com/en/core/AtomS3%20Lite | Grove ports on the m5stack boards have built in level shifters |

Cable
* Get one of the grove female cable and a ST PAP-05V-S connector. Remove one end of the grove connector and replace it with a ST PAP-05V-S connector. Here's an example:
![image](https://github.com/gekkekoe/ecodan-esp32/blob/main/img/m5stack_cn105.jpg?raw=true)

Pin mapping (from left to right)
| grove | cn105 |
|:---|:---|
|pin 1 - black (gnd) | pin 4 - black (gnd) |
|pin 2 - red (5v) | pin 3 - red (5v) |
|pin 3 - white (GPIO 2) | pin 2 - white (Tx) |
|pin 4 - yellow (GPIO 1) | pin 1 - yellow (Rx) |

*Note: pin 5 (12v) on the cn105 is not used.*

# required firmware
One of the following:
1) ecodan-ha-local
2) esphome-ecodan-heatpump

# 1) build ecodan-ha-local firmware
* Get visual studio code with the platformIO plugin
* Use https://github.com/gekkekoe/ecodan-esp32/blob/main/ecodan-ha-local/platform.ini to build with platformIO. Find your definition https://docs.platformio.org/en/latest//boards/ and set it in the platform.ini. Please note that the esp32-s3 is only supported in platformIO version >= 6.
* Flash the firmware to the esp32
* Power down (!) the heatpump and connect the grove-cn105 and esp32
* Power up the heatpump 
* find the ip assigned to the ecodan-ha-local and access the configuration via http
* Configure Tx, Rx pins (my heatpump Xxxx-vm2d with controller unit FTC6 seems to have the Rx/Tx swapped, if the esp cannot connect to the heatpump, swap the Rx/Tx in the configuration), led indicator (pin 35), mqtt and wifi. Once rebooted, it should be auto discovered in home assistant. For more details on how to configure ecodan-ha-local please read https://github.com/rbroker/ecodan-ha-local/blob/main/README.md

# 2) build esphome-ecodan-heatpump firmware
If you want to manage the ecodan esphome from home assistant, add the esphome add-on (https://esphome.io/guides/getting_started_hassio.html). You will need the api key for `secrets.yaml`. For more detailed info: https://github.com/tobias-93/esphome-ecodan-heatpump
### Build via home assistant
https://github.com/tobias-93/esphome-ecodan-heatpump

### Build via cmd line:
* Install ESPHome https://esphome.io/guides/getting_started_command_line.html
    ```console
    python3 -m venv venv
    source venv/bin/activate
    pip3 install wheel
    pip3 install esphome
    ```
* Fill in `secrets.yaml` and copy the `ecodan-esphome-esp32s3.yaml` to your esphome folder and edit the values (*check GPO pins (uart: section), you might need to swap the pins in the config*)
* Build
```console
esphome compile ecodan-esphome-esp32s3.yaml
```
* To find the tty* where the esp32 is connected at, use `sudo dmesg | grep tty`. On my machine it was `ttyACM0` for usb-c, and `ttyUSB0` for usb-a.
* Connect your esp32 via usb and flash
```console 
esphome upload --device=/dev/ttyACM0 ecodan-esphome-esp32s3.yaml
```
* You can update the firmware via de web interface of the esp after the initial flash, or use the following command to flash over the network
```console 
esphome upload --device=ip_address ecodan-esphome-esp32s3.yaml
```

Here's how it's connected inside my heatpump:

![image](https://github.com/gekkekoe/ecodan-esp32/blob/main/img/m5stack_installed.jpg?raw=true)
