# Using Proxy setup
There are a few options to connect a melcloud wifi adapter / procon as slave.
The slave works as normal and you are able to control the heatpump via the esp. Note that the older `PAC-WF010-E` is not supported.

### Custom proxy pcb
- The recommended hardware for running this integration (in both proxy and non-proxy) is the [Asgard PCB](https://github.com/gekkekoe/esphome-ecodan-hp/blob/main/asgard/README.md).

![image](https://github.com/gekkekoe/esphome-ecodan-hp/blob/main/asgard/img/case-pcb.png?raw=true) ![image](https://github.com/gekkekoe/esphome-ecodan-hp/blob/main/asgard/img/case-top.png?raw=true) 

- Alternatively you can use the proxy pcb by
[@fonkse](https://github.com/gekkekoe/esphome-ecodan-hp/discussions/70#discussioncomment-11133291)

Select the `esp32s3-proxy2.yaml` as configuration in `ecodan-esphome.yaml`. Build and flash the firmware.

### Generic esp32 with two pair of RX/TX GPIO. 
I will be using a m5stack atom s3 lite as an example.
Use the following schematics to create the connectors.

![image](https://github.com/gekkekoe/esphome-ecodan-hp/blob/main/img/atoms3-proxy.png?raw=true)

|           | atoms3lite      |              |
|-----------|-----------------|--------------|
|           | grove female    | socket: B05B-PASK-1, connector: ST PAP-05V-S |
|           | pin header      | socket: B05B-PASK-1, connector: ST PAP-05V-S |

We do need to connect the 12v to the slave port.

Select the `esp32s3-proxy.yaml` as configuration in `ecodan-esphome.yaml`. Build and flash the firmware.
