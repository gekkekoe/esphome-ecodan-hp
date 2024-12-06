# Using Proxy setup
There are a few options to connect a melcloud wifi adapter / procon as slave.
The slave works as normal and you are able to control the heatpump via the esp.

### Custom proxy pcb
[The easiest option is to use a custom pcb by @fonkse](https://github.com/gekkekoe/esphome-ecodan-hp/discussions/70#discussioncomment-11133291)

@fonkse created a pcb + wires and it is basically plug and play. This is the recommended hardware setup for proxy.

![image](https://github.com/gekkekoe/esphome-ecodan-hp/blob/main/img/proxy-setup.png?raw=true)

Select the `esp32s3-proxy2.yaml` as configuration in `ecodan-esphome.yaml`. Build and flash the firmware.

### Connecting and setup
This guide is written for those with no experience of ESPHome and assumes only a basic understanding of Home Assistant. For those with experience of both then you’ll want to jump straight to step 2 for where to plug in the cable.


You may have come across this device from forums etc but don’t really understand how it works. The cable is supplied with a microcomputer known as an M5 Atom S3 Lite fitted with a ESP32 chip. The cable can be used for local network (non-cloud) control of a Mitsubishi Ecodan heat pump fitted with a Flow Temperature Controller (FTC) which is the white metal box often fitted to the front of the water cylinder. The cable’s standout feature is that is also allows you to use MELCloud as you would normally.

 
![image](https://github.com/fonske/esphome-ecodan-hp/blob/main/img/proxy2.jpg?raw=true)

How to install:

 

The cable needs to be connected to the control board of the heat pump within the FTC case. Before opening the case it must be isolated and you should be aware there are different ways of powering the controller so make sure it’s completely dead first! If you have an immersion heater installed this will also need to be isolated as the contactor for that is within the case but is powered by a separate circuit.

 ![image](https://github.com/fonske/esphome-ecodan-hp/blob/main/img/connection_FTC?raw=true)

One end of the supplied cable needs to be inserted into the ‘INT’ side of the Atom and the other end inserted into the RED coloured connector of the FTC control board as shown below. If you are using MELCloud and therefore have a WiFi adaptor this obviously needs to be disconnected first and the connector inserted into the ‘EXT’ side of the Atom. Consider mounting the Atom outside of the case for a stronger WiFi connection and easier future access. Note: if installed, do not disconnect and use the white (top) connector by mistake as this is for the wireless remote control receiver (room temperature controller).


The Atom is supplied with the firmware already installed so all you need to do is close the FTC case and power up the unit. The Atom will now be in WiFi hotspot mode to allow you to connect it to your home network. See https://esphome.io/components/captive_portal.html for details.
 

Now go to Home Assistant and install the ESPHome integration. With the Atom on your WiFI network it will appear as a new device and can be added in the usual way.
 

Note: For those new to GitHub who have read the instructions for how to make up a cable and flash an Atom with the necessary firmware – do not follow these instructions as this has already been done for you! Indeed the main repository does not include proxy support which means that if you do overwrite the installed firmware then MELCloud will not work.


