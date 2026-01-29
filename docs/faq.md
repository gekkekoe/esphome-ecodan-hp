# FAQ: Common Issues

## 1. WIFI: Why does it disconnect on bad signal?

The ESP32 (the hardware running ESPHome) is sensitive to Wi-Fi signal quality. In the context of the Ecodan integration, disconnects on "bad signal" generally happen for the following reasons:

* **Signal Threshold:** If the RSSI (Received Signal Strength Indicator) drops below roughly **-75dBm**, the ESP32 radio struggles to maintain a stable connection. Unlike a phone or laptop, the ESP32 has a smaller antenna and less error-correction capability.
* **Power Save Mode:** By default, ESPHome may try to enter a light sleep/power-save mode to reduce heat and power usage. On a network with a weak signal, the "wake up" packet might be missed, causing the Access Point to drop the client.


**Solution:** 
* Ensure your RSSI is better than -60dBm (closer to zero is better). You may need to reposition the ESP unit outside the metal casing of the heat pump, which acts as a Faraday cage blocking the signal.
* Pin ESP to the nearest Access Point

**Note:** If the ESP cannot connect to the wifi 3 times over a time, it will fallback to Access Point Mode. You need to connect to the ESP hotspot to re-configure the wifi.


## 2. Why does the heat pump cycle (Short Cycling)?

Short cycling occurs when the heat pump turns on and off frequently (e.g., running for only 5-10 minutes). This happens due to a mismatch between the heat pump's **Minimum Output Capacity** and the house's **Heat Demand**.

* **Physics (Mild Weather):** In mild weather (e.g., 12°C outside), your house might only need 1kW of heat to stay warm. If your Ecodan's *minimum* output is 4kW, it produces 4x more heat than needed. The water heats up instantly, hits the target temperature, and the unit shuts down to prevent overheating. Check the Ecodan databook for min output levels for your unit.
* General tips on avoiding short cycles: 
    * The heatpump cycles if the following equation is false: `Output heatpump (kW) <= flow rate / 60 * (feed temp - return temp) * 4.18`
    * Increase flow rate to increase output (via pump speed setting)
    * Increase delta T (via feed temp) to increase output
    * Increase the receiving side by opening more/all circuits. Ensure that the largest circuit is always open. Don't allow small circuits to request demand on their own.
* **Mitigation in this Integration:** The firmware includes **"Short Cycle Mitigation"** and **"Auto Adaptive"** features:
    * It monitors the "Delta" (difference) between the requested flow temp and actual flow temp.
    * If it detects a "High Delta" (rapid rise in temp), it predicts a short cycle is about to happen.
    * It can apply a **"Predictive Boost"** (raising the target temp slightly) to force the unit to run longer and store extra heat in your floor/radiators, effectively using your floor as a battery.
    * If a short cycle is unavoidable, it enforces a **"Lockout"** period where the pump is held off for a set time (e.g., 30 mins) to protect the compressor.
* If your unit is oversized then consider heating in blocks of couple of hours. Turn off heating at night to let the floor cool down.

## 3. Why is some data not available in Proxy Mode?

In **Proxy Mode**, the ESP32 acts as a "Man-in-the-Middle" or passive listener between the Heat Pump and another controller (like the official MelCloud Wi-Fi adapter).

* **Passive Observer:** The ESP will not ask (poll) the heat pump for data because doing so would collide with the signals from the main controller (MelCloud). It will only "listen" to the conversation that is already happening.
* **Missing Packets:** If the MelCloud adapter doesn't ask for a specific piece of data (e.g., "Outdoor Unit Thermistors" or "Energy Consumption"), the heat pump never sends it, and therefore the ESP never sees it.

## 4. Why are some sensors unavailable on my unit?

Outdoor unit sensors are not always available by default. Retrieving them requires service calls which take a long time (~5s) and block other communications. Therefore, I only query a limited set of sensors: `COMPRESSOR_STARTS`, `TH4_DISCHARGE_TEMP`, `TH3_LIQUID_PIPE1_TEMP`, `DISCHARGE_SUPERHEAT`, and `FAN_SPEED`.

Newer FTC7+ systems can also report `TH6_2_PHASE_PIPE_TEMP`, `TH32_SUCTION_PIPE_TEMP`, `TH8_HEAT_SINK_TEMP`, and `SUB_COOL` without requiring service calls. These will be automatically exposed if available.

## 5. How can I monitor logs?

Download/save the [logviewer.html](https://raw.githubusercontent.com/gekkekoe/esphome-ecodan-hp/refs/heads/main/docs/logviewer.html) to your computer locally. Open `logviewer.html` with a browser and connect to the ESP ip. It should stream logging and you will be able to save it to a file.