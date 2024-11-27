# Using Proxy setup
There are two options to connect a melcloud wifi adapter / procon as slave.
The slave works as normal and you are able to control the heatpump via the esp.

### Custom proxy pcb
[The easiest optionn is to use a custom pcb by @fonkse](https://github.com/gekkekoe/esphome-ecodan-hp/discussions/70#discussioncomment-11133291)

@fonkse created a pcb + wires and it is basically plug and play. This is the recommended hardware setup for proxy.

![image](https://github.com/gekkekoe/esphome-ecodan-hp/blob/main/img/proxy-setup.png?raw=true)

### Heishamon large V5
[Another option is to use a Heishamon v5 (large).](https://www.tindie.com/products/thehognl/heishamon-communication-pcb/
)
 This option also allows you to use ethernet instead of wifi and has two relay switches available that can be used.
Use the following diagram to create the connectors. 

![image](https://github.com/gekkekoe/esphome-ecodan-hp/blob/main/img/heishamon-proxy.png?raw=true)

Heishamon has two connectors (large and small).
The large connector (bottom) is connected to the heatpump and the small (top) one is connected to the slave.

|                 | heishamon       |              |
|-----------------|-----------------|--------------|
| large connector | socket: B05B-XASK-1, connector: XAP-05V-1       | socket: B05B-PASK-1, connector: ST PAP-05V-S |
| small connector | socket: B05B-PASK-1, connector: ST PAP-05V-S    | socket: B05B-PASK-1, connector: ST PAP-05V-S |

Alternatively you can also rewire the melcloud wifi adapter / procon connector to match the pin layout and plug it into the small socket on the heishamomn board (You still need to create a cable for the large connector).

NOTE: The current version of heishamon V5 requires a 10kΩ pull-up resistor on the RX line from the heat pump. It's recommended to solder it at the back side of the board.

![image](https://github.com/gekkekoe/esphome-ecodan-hp/blob/main/img/solder.png?raw=true)

Select the `esp32s3-heisamon.yaml` as configuration in `ecodan-esphome.yaml`. Build and flash the firmware. Connect the small connector to the slave and the large connector to the heatpump.

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
