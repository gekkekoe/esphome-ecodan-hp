# Auto-Adaptive Control

The **Auto-Adaptive Control** feature transforms your Ecodan heat pump into a smart, self-learning system. It intelligently adjusts the heat pump's operation to match the unique thermal properties of your home, maximizing both comfort and energy efficiency.

When enabled, this feature continuously monitors the room temperature and compares it to the setpoint. It then fine-tunes the heat pump's flow temperature to correct any persistent errors, ensuring your home is always perfectly comfortable using the minimum amount of energy.

The system is designed to be highly robust and accounts for real-world scenarios such as:
* **System Saturation**: It pauses the learning process if the heat pump is already running at 100% capacity.
* **Defrost Cycles**: It intelligently ignores temperature data during defrost cycles to avoid drawing incorrect conclusions.
* **Short-Cycling**: It works alongside the **Short-Cycle Mitigation** feature to ensure stable operation.

---

## Persistent Learning

A significant advantage of this system is that all learned parameters, specifically the crucial **`heating_curve_offset`** and **`cooling_curve_offset`**, are automatically saved to the ESP32's flash memory.

Unlike the native Mitsubishi auto-adaptive function, which can lose its learned data after a power cycle, this implementation ensures that a reboot or power outage **does not cost time to re-learn**. The system immediately resumes operation with its last known optimal settings, ensuring continuous efficiency.

---

## Important Note on Operating Mode

For this feature to function correctly, the heat pump must be set to a **Fixed Flow Temperature** mode for both heating and cooling. The Auto-Adaptive algorithm takes over the role of the built-in heating curve by dynamically adjusting this fixed setpoint.

---

## Proactive Control with Setpoint Bias

The **`Auto-Adaptive: Setpoint Bias`** parameter unlocks the controller's most advanced capability: **proactive operation**. While the core algorithm is highly efficient at *reacting* to current conditions, the Setpoint Bias allows it to *anticipate* future events.

A "bias" is a temporary, externally controlled adjustment to the primary setpoint. By applying a positive or negative bias, an external system (like Home Assistant) can instruct the controller to pre-heat or pre-cool the house in response to upcoming events.



This transforms your controller from a reactive system into a predictive one, enabling strategies like:
* **Load Shifting**: Pre-heating the house with a positive bias (e.g., `+1.0°C`) during off-peak electricity hours and then applying a negative bias (e.g., `-1.5°C`) during expensive peak hours. The thermal mass of your home acts as a battery.
* **Solar Gain Compensation**: Applying a negative bias (e.g., `-0.7°C`) on a cold but sunny morning to prevent the system from overheating the house, letting the "free" energy from the sun do the work instead.

---

## Configuration Parameters

All parameters are adjustable in real-time from the Home Assistant interface.

| Parameter                                     | Description                                                                          | Guidance & Default                                                                                                                              |
| --------------------------------------------- | ------------------------------------------------------------------------------------ | ----------------------------------------------------------------------------------------------------------------------------------------------- |
| **`Auto-Adaptive: Control`** | **Enables or disables the entire Auto-Adaptive feature.** When disabled, the system will revert to using the standard fixed flow temperature setpoints. | **Default**: `On`                                                                                                                               |
| **`Auto-Adaptive: Heating System Type`** | Tunes the algorithm's behavior to match your system's thermal inertia (response time). | **Default**: `Underfloor Heating`<br>• **UFH**: For slow, high-inertia systems.<br>• **UFH + Radiators**: For hybrid systems.<br>• **Radiators**: For fast, low-inertia systems. |
| **`Auto-Adaptive: Heating Curve Slope`** | Determines how aggressively the flow temp rises as the outside temp drops.             | **Default**: `1.5`<br>• **Low (0.6-0.8)** for well-insulated homes with UFH.<br>• **High (1.2-1.6)** for older homes with radiators.          |
| **`Auto-Adaptive: Cooling Curve Slope`** | Determines how aggressively the flow temp drops as the outside temp rises.             | **Default**: `1.2`<br>• **Low (0.8-1.2)** for homes with good sun protection.<br>• **High (1.8-2.5)** for homes with high solar gain.          |
| **`Auto-Adaptive: Max. Heating Flow Temperature`**| Sets a hard safety limit for the flow temperature during heating to protect floors.      | **Default**: `38.0°C`                                                                                                                           |
| **`Auto-Adaptive: Min. Cooling Flow Temperature`**| Sets a hard safety limit for the flow temperature during cooling to prevent condensation. | **Default**: `18.0°C`                                                                                                                           |
| **`Auto-Adaptive: Cooling Smart Start Temp`** | Sets an "efficiency ceiling" for cooling. The system will never start cooling with a flow temperature *higher* than this value, preventing inefficient cycles on mild days. | **Default**: `19.0°C`<br>Must be ≥ `Min. Cooling Flow Temperature`. |
| **`Auto-Adaptive: Setpoint Bias`** | Applies a temporary adjustment to the target temperature for proactive control. | **Default**: `0.0°C`<br>A range of **-1.5°C to +1.5°C** is effective for UFH, while **-2.5°C to +2.5°C** can be used for radiators. |
| **`Auto-Adaptive: Room Temperature source`** | Selects the source for the **current** room temperature. The **target** temperature is always read from the active Ecodan thermostat. | **Default**: `Room Thermostat`<br>• **Room Thermostat**: Uses the current temperature from the Ecodan thermostat.<br>• **Rest API**: Overrides the current temperature with an external sensor. E.g.:<br>`curl -X POST "http://<esp_ip>/number/temperature_feedback/set?value=21.5"`|

---

## Fine-Tuning Initial Values in YAML

While the system learns automatically, providing a good starting point in your YAML configuration is crucial for immediate efficiency. This is especially true for the **`heating_curve_offset`**, which is the baseline for the heating curve.

| System Type                      | Recommended `initial_value` for `heating_curve_offset` |
| -------------------------------- | -------------------------------------------------------- |
| **Underfloor Heating** | `23.0` (More efficient for well-insulated homes)       |
| **Underfloor Heating + Radiators** | `26.0` (A good intermediate value)                       |
| **Radiators** | `30.0` (A common baseline for radiator systems)          |