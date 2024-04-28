# ESPHome version of Ecodan ha local
It is highly inspired by https://github.com/rbroker/ecodan-ha-local, https://github.com/tobias-93/esphome-ecodan-heatpump and https://github.com/vekexasia/comfoair-esp32

# required hardware
If you don't want to solder, use one of the boards that supports 5v on the GPIO ports (basically all m5stack boards with a grove connector (HY2.0-4P)). It also should work for airco units with cn105 connectors.

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

* Edit the following section in `ecodan-esphome.yaml` to match your configuration (esp board, zone1/zone2)

```
packages:
  esp32: !include confs/esp32s3.yaml # esp32.yaml for regular board
  zone1: !include confs/zone1.yaml
## disable if you don't want to use zone 2
  zone2: !include confs/zone2.yaml
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
esphome upload --device=ip_address ecodan-esphome.yaml
```

Here's how it's connected inside my heatpump:

![image](https://github.com/gekkekoe/esphome-ecodan-hp/blob/main/img/m5stack_installed.jpg?raw=true)

The esphome component will be auto detected in Home Assistant:

![image](https://github.com/gekkekoe/esphome-ecodan-hp/blob/main/img/ha-integration.png?raw=true)


[!["Buy Me A Coffee"](https://www.buymeacoffee.com/assets/img/custom_images/orange_img.png)](https://www.buymeacoffee.com/gekkekoe)
