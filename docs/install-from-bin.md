# easy install
## Get the factory.bin firmware for initial flash
* Go to https://github.com/gekkekoe/esphome-ecodan-hp/releases/latest and download one of the factory.bin files. 
```
Firmware for Atom S3 (Lite): esp32s3-yy-xx-version.factory.bin (where xx = language, yy = number of supported zones)
Firmware for Atom S3 (Lite) with proxy PCB: esp32s3-proxy2-yy-xx-version.factory.bin (where xx = language, yy = number of supported zones)
```
* Go to https://web.esphome.io/?dashboard_install (Only works in Chrome or Edge), connect the atom via usb-c (disconnect from the cn105 port before connecting to the pc) and click connect. Chrome will let you select the detected esp. Finally you click install and select the factory.bin firmware (downloaded in previous step) and flash. 

* Configure WiFi in https://web.esphome.io/?dashboard_install: If you have a recent firmware you can set the WiFi credentials via the three dots menu / Confgure WiFi / Change WiFi. You can skip the next step if the WiFi is configured this way (wait for the led indicator to become blue). 

* Configure WiFi via Hotspot: Power the esp via usb-c. The led indicator will turn blue indicating that its in access point mode. Connect to the wifi network `ecodan-heatpump` with password `configesp`. Fill in your wifi credentials and click save (http://ecodan-heatpump.local or http://ip). The esp will reboot and the led will become green indicating that the wifi is connected correctly. 

* Disconnect the unit from the computer and connect it to the heatpump via the CN105 connector (don't forget to power down the unit via circuit breaker!). You will get notifications if an update is available and its updateable in Home Assistant itself. 
