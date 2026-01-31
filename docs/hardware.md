# Recommended hardware
If you don't want to solder, use one of the tested boards. [More boards that should be working](https://github.com/SwiCago/HeatPump/issues/13#issuecomment-457897457). It also should work for airco units with cn105 connectors. 

The recommended hardware for running this integration is the [Asgard PCB](https://github.com/gekkekoe/esphome-ecodan-hp/blob/main/asgard/README.md).

![image](https://github.com/gekkekoe/esphome-ecodan-hp/blob/main/asgard/img/case-pcb.png?raw=true) ![image](https://github.com/gekkekoe/esphome-ecodan-hp/blob/main/asgard/img/case-top.png?raw=true) 

# Tested hardware

| Board | Link | Notes |
|:---|:----:|:---|
| Asgard | https://github.com/gekkekoe/esphome-ecodan-hp/blob/main/asgard/README.md | Plug & Play / supports proxy |
| M5Stack Atom Lite (ESP32 variants) | https://docs.m5stack.com/en/core/ATOM%20Lite | Grove ports used |(confs/m5stack-atom-lite-proxy.md) |
| M5Stack Atom Lite S3 (ESP32-S3 variants) | https://docs.m5stack.com/en/core/AtomS3%20Lite | Grove ports used |


# Grove to CN105 cable
Only needed when using Atom S3 Lite, For Asgard use the supplied cable.
* Get one of the grove female cable and a ST PAP-05V-S connector. Remove one end of the grove connector (you can lift the clamp a bit and pull out the wire) and replace it with a ST PAP-05V-S connector. 

Pin mapping (image pin layout: from left to right)
| grove | cn105 |
|:---|:---|
|pin 1 - black (gnd) | pin 4 - black (gnd) |
|pin 2 - red (5v) | pin 3 - red (5v) |
|pin 3 - white (GPIO 2) | pin 2 - white (Tx) |
|pin 4 - yellow (GPIO 1) | pin 1 - yellow (Rx) |

When done correctly, it should look like [this](https://raw.githubusercontent.com/gekkekoe/esphome-ecodan-hp/refs/heads/main/img/m5stack_cn105.jpg)

*Note: pin 5 (12v) on the cn105 is not used.*

*Note2: when using 5v from the cn105, **do not** use an external usb power source at the same time*
