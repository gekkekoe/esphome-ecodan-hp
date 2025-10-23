# build esphome-ecodan-hp firmware from source
### Build via cmd line:
* Install ESPHome https://esphome.io/guides/getting_started_command_line.html. (Use [Python <= 3.12](https://github.com/esphome/issues/issues/6558) to avoid build errors on Windows)
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
            confs/base.yaml,            # required
            confs/request-codes.yaml,   # disable if your unit does not support request codes (service menu)
            confs/esp32s3.yaml,         # confs/esp32.yaml, for regular board
            #confs/esp32s3-led.yaml,    # atom s3 status leds
            confs/zone1.yaml,
            confs/server-control-z1.yaml,
            ## enable if you want to use zone 2
            #confs/zone2.yaml,
            #confs/server-control-z2.yaml,
            ## enable label language file
            confs/ecodan-labels-en.yaml,
            #confs/ecodan-labels-nl.yaml,
            #confs/ecodan-labels-it.yaml,
            #confs/ecodan-labels-fr.yaml,
            #confs/ecodan-labels-de.yaml,
            #confs/ecodan-labels-fi.yaml,
            #confs/debug.yaml,
            confs/wifi.yaml
           ]
```

* Build
```console
esphome compile ecodan-esphome.yaml
```
* To find the tty* where the esp32 is connected at, use `sudo dmesg | grep tty`. On my machine it was `ttyACM0` for usb-c, and `ttyUSB0` for usb-a. `tty.usbmodemxxx` for Mac and `COMXX` for Windows

* Connect your esp32 via usb and flash
```console 
esphome upload --device=/dev/ttyACM0 ecodan-esphome.yaml
```
* You can update the firmware via the web interface of the esp after the initial flash, or use the following command to flash over the network
```console 
esphome upload --device ip_address ecodan-esphome.yaml
```
