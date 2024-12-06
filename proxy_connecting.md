# Connecting and installing proxy setup

[The easiest option is to use a custom pcb by @fonkse](https://github.com/gekkekoe/esphome-ecodan-hp/discussions/70#discussioncomment-11133291)

@fonkse created a pcb + wires and it is basically plug and play. This is the recommended hardware setup for proxy.

![image](https://github.com/gekkekoe/esphome-ecodan-hp/blob/main/img/proxy-setup.png?raw=true)

Select the `esp32s3-proxy2.yaml` as configuration in `ecodan-esphome.yaml`. Build and flash the firmware.

### Connecting and setup
This guide is written for those with no experience of ESPHome and assumes only a basic understanding of Home Assistant. 

You may have come across this device from forums etc but don’t really understand how it works. The cable is supplied with a microcomputer known as an M5 Atom S3 Lite fitted with a ESP32 chip. The cable can be used for local network (non-cloud) control of a Mitsubishi Ecodan heat pump fitted with a Flow Temperature Controller (FTC) which is the white metal box often fitted to the front of the water cylinder. The cable’s standout feature is that is also allows you to use MELCloud as you would normally.

 
## How to install:

1. The cable needs to be connected to the control board of the heat pump within the FTC case. Before opening the case it must be isolated and you should be aware there are different ways of powering the controller so make sure it’s completely dead first! If you have an immersion heater installed this will also need to be isolated as the contactor for that is within the case but is powered by a separate circuit.

 ![image](https://github.com/fonske/esphome-ecodan-hp/blob/main/img/connection_FTC.jpg?raw=true)

2. One end of the supplied cable needs to be inserted into the ‘INT’ side of the Atom and the other end inserted into the RED coloured connector of the FTC control board as shown below. If you are using MELCloud and therefore have a WiFi adaptor this obviously needs to be disconnected first and the connector inserted into the ‘EXT’ side of the Atom. Consider mounting the Atom outside of the case for a stronger WiFi connection and easier future access. Note: if installed, do not disconnect and use the white (top) connector by mistake as this is for the wireless remote control receiver (room temperature controller).

![image](https://github.com/fonske/esphome-ecodan-hp/blob/main/img/proxy2_atom.jpg?raw=true)

3. The Atom is supplied with the firmware already installed so all you need to do is close the FTC case and power up the unit. The Atom will now be in WiFi hotspot mode to allow you to connect it to your home network. See https://esphome.io/components/captive_portal.html for details.
 

4. Now go to Home Assistant. With the Atom on your WiFI network it will appear as a new device and can be added in the usual way.
 

Note: For those new to GitHub who have read the instructions for how to make up a cable and flash an Atom with the necessary firmware – do not follow these instructions as this has already been done for you! Indeed the main repository does not include proxy support which means that if you do overwrite the installed firmware then MELCloud will not work.

## Flash it yourself.
If you want to do flash it yourself, install the ESPHome integration. Choose new device, press continue, give it a name, for example ecodan-esphome, choose for example esp32-s3. DO NOT CHOOSE ENCRYPTION-KEY (skip)
You need to fill in (wifi) secrets (in the upper right corner of the esphome dashboard).
Example of the code is here:

```
# Your Wi-Fi SSID and password
wifi_ssid: "wifi network id"
wifi_password: "wifi password"
```

And click save.
Now go to the new device and choose EDIT.
Copy the raw code from githubs ecodan-esphome.yaml into this new device and overwrite ALL existing code.
Instead of the `esp32s3.yaml` Fill in `esp32s3-proxy2.yaml` as configuration in `ecodan-esphome.yaml`.

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
            confs/server-control.yaml,
            #confs/debug.yaml,
            ## enable this to monitor WiFi status with ESP in-built LED
            #confs/status_led.yaml,
            ## enable this to monitor status with custom led colors, uses https://github.com/esphome/esphome/pull/5814
            #confs/status_led_rgb.yaml,
            confs/wifi.yaml
           ]
```

Click save and install.
Choose manual and the code will be compiled to a ecodan-heatpump.bin. Choose Factory format when compiling is finished.
You can then flash the .bin file, for the first time, by using an usb 'c' cable connected to the Atom S3 lite. (need to power off the heatpump again and remove the m5stack proxy to flash it)
Flash the atom connected to a usb cable by using: https://web.esphome.io/?dashboard_install
Click connect and choose the correct com port. And then click install.
Next time, the firmware can be updated by wifi directly, from within esphome, as it detects the device is already setup in your network, with the same code.



