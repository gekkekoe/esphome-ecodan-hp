# Connecting and installing proxy setup

> [!TIP]
> [The easiest option is to use a custom pcb by @fonkse](https://github.com/gekkekoe/esphome-ecodan-hp/discussions/70#discussioncomment-11133291)

@fonkse created a pcb + wires and it is basically plug and play. This is the recommended hardware setup for proxy.

![image](https://github.com/gekkekoe/esphome-ecodan-hp/blob/main/img/proxy-setup.png?raw=true)

Select the `esp32s3-proxy2.yaml` as configuration in `ecodan-esphome.yaml`. Build and flash the firmware.

### Connecting and setup
This guide is written for those with no experience of ESPHome and assumes only a basic understanding of Home Assistant. 

Used abbreviations:
FTC Flow Temperature Controller (main controller normally fitted on the inside unit)
 
## How to install:

1. The cable needs to be connected to the main board of the heat pump within the inside unit on the brown connector CN105.

> [!WARNING]
> Before opening the unit ensure that there is no power going into the unit.
> Switch off heatpump and then turn off circuit breakers to heatpump.

 ![image](https://github.com/gekkekoe/esphome-ecodan-hp/blob/main/img/connection_FTC.jpg?raw=true)

2. Connect the cable from the heatpump to the port labeled 'INT' (disconnect melcloud wifi adapter from the main board if it was connected). Connect the melcloud wifi adapter to the 'EXT' port.
   
![image](https://github.com/gekkekoe/esphome-ecodan-hp/blob/main/img/proxy2_atom.jpg?raw=true)

3. If the Atom is pre-flashed, you are basically done. Close the unit and restore the power. The Atom will go into WiFi hotspot mode to allow you to connect it to your home network. See https://esphome.io/components/captive_portal.html for details.
 

4. Now go to Home Assistant. With the Atom on your WiFI network it will appear as a new device and can be added in the usual way.

## Flash it yourself.
> [!WARNING]
> NEVER connect a usb cable to the ATOM S3 lite ESP WHEN connected to the heatpump..
> 
> Switch OFF heatpump and then turn off circuit breakers to heatpump and remove the ESP first.

If you want to do flash it yourself, install the ESPHome integration as an addon in home assistant. 
1. Choose new device and press continue
2. Give it a name, for example ecodan-esphome
3. Choose for example esp32-s3
4. A configuration will be added
> [!IMPORTANT]
> DO NOT CHOOSE ENCRYPTION-KEY (skip)

5. You need to fill in (wifi) secrets (in the upper right corner of the esphome dashboard).
```
# Your Wi-Fi SSID and password
wifi_ssid: "wifi network id"
wifi_password: "wifi password"
```

6. Click save.
7. Now go to the new device and choose EDIT.
8. Copy the raw code from githubs [ecodan-esphome.yaml](ecodan-esphome.yaml) into this new device and overwrite ALL existing code.
9. Instead of the `esp32s3.yaml` Fill in `esp32s3-proxy2.yaml` as configuration in `ecodan-esphome.yaml`.
> [!TIP]
> Example packages code with esp32s3-proxy2.yaml:
```
packages:
  remote_package:
    url: https://github.com/gekkekoe/esphome-ecodan-hp/
    ref: main
    refresh: always
    files: [ 
            confs/base.yaml,        # required
            confs/esp32s3-proxy2.yaml,     # confs/esp32.yaml, for regular board
            confs/zone1.yaml,
            ## enable if you want to use zone 2
            #confs/zone2.yaml,
            ## enable label language file
            confs/ecodan-labels-en.yaml,
            #confs/ecodan-labels-nl.yaml,
            #confs/ecodan-labels-it.yaml,
            #confs/ecodan-labels-fr.yaml,
            #confs/ecodan-labels-es.yaml
            confs/server-control.yaml,
            #confs/debug.yaml,
            ## enable this to monitor WiFi status with ESP in-built LED
            #confs/status_led.yaml,
            ## enable this to monitor status with custom led colors, uses https://github.com/esphome/esphome/pull/5814
            #confs/status_led_rgb.yaml,
            confs/wifi.yaml
           ]
```
10. Click save and install.
11. Choose manual download and the code will be compiled to a ecodan-heatpump.bin file
12. Choose Factory format when compiling is completed
13. You can then flash the .bin file, for the first time, by using an usb 'c' cable connected to the Atom S3 lite. (need to power off the heatpump again and remove the m5stack proxy to flash it with USB cable)
14. Flash the atom connected to a usb cable by using  [Esphome Web](https://web.esphome.io/?dashboard_install)
15. Click connect and choose the correct com port. And then click install.
16. Disconnect from usb cable and install back inside the heatpump on CN105
17. All done!

> [!TIP]
> Next time, the firmware can be updated through wifi connection directly, from within esphome, as it detects the device and is already setup in your network, with the same esphome code.
