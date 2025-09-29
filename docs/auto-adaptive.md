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

# Getting Started with Auto-Adaptive Control

## Configuration Steps

### Step 1: Physical Thermostat Connection (If Applicable)

This guide assumes a common scenario where an external room thermostat (like a Tado, Nest, etc.) is wired to the **IN1** terminal on your heat pump's FTC board. This thermostat is responsible for sending the main heat request signal to the heat pump. It should work with the remote wireless thermostat as well.

### Step 2: Set Up the Temperature Feedback Loop

The Auto-Adaptive algorithm needs to know the **current temperature** being measured by your external thermostat. You must send this value back to the ESPHome controller.

You can do this in two ways:

* **Via Home Assistant UI**: Create an automation in Home Assistant that copies the temperature from your thermostat's sensor to the `Auto-Adaptive: Current Room Temperature Feedback` number entity.
* **Via REST API**: Use an external system or script to post the temperature directly.

Example REST API call to set the feedback temperature to 21.5°C:
```bash
curl -X POST "http://<esp_ip>/number/temperature_feedback/set?value=21.5"
```

Example Home Assistent automation:
```yaml
alias: Sync Room Temp to Auto-Adaptive Controller
triggers:
  - trigger: state
    entity_id: climate.kantoor # Replace with your climate entity
    attribute: current_temperature
conditions:
  - condition: template
    value_template: >-
      {{ trigger.from_state.attributes.current_temperature !=
      trigger.to_state.attributes.current_temperature }}
actions:
  - action: number.set_value
    target:
      entity_id: number.ecodan_heatpump_auto_adaptive_current_room_temperature_feedback
    data:
      value: "{{ trigger.to_state.attributes.current_temperature }}"


```

### Step 3: Configure Initial Parameters

In Home Assistant, navigate to your dashboard and set the initial parameters for the controller on the **"Auto-Adaptive Settings"** tab.



1.  **Set the Heating System Profile**: Choose the option from the `Auto-Adaptive: Heating System Type` dropdown that best matches your home (`Underfloor Heating`, `Underfloor Heating + Radiators`, or `Radiators`).
2.  **Set the Heating Curve Slope**: Adjust the `Auto-Adaptive: Heating Curve Slope` slider. A good starting point for underfloor heating is `0.7`-`0.8`, while radiators often need a steeper slope like `1.4`-`1.6`.
3.  **Set Safety Limits**: Adjust the `Auto-Adaptive: Max. Heating Flow Temperature` slider to a value that is safe for your floors (e.g., `38.0°C` for UFH). Do the same for the cooling limits.

### Step 4: Activate the System

Now you are ready to let the algorithm take control.

1.  **Set Heat Pump to Fixed Flow Mode**: On your main Mitsubishi thermostat or control panel, ensure that the active zone(s) are set to **Fixed Flow Temperature** mode for heating (and cooling, if applicable). This is critical, as it allows the ESPHome controller to have full control.
2.  **Enable Auto-Adaptive Control**: In Home Assistant, turn **ON** the `Auto-Adaptive: Control` switch.

---

## Monitoring Operation

Once enabled, the system is now active. The algorithm will run every 10 minutes to calculate and set a new optimal flow temperature.

You can monitor the learning process by viewing the history of these key sensors in Home Assistant:

* **`Auto-Adaptive: Heating Offset (Z1/Z2)`**: This is the most important sensor to watch. You will see this value slowly change over hours and days as the system fine-tunes itself. If it's trending down, the system is learning to be more efficient. If it's trending up, it's learning to be more powerful.
* **`Auto-Adaptive: Dynamic Learning Rate`**: This sensor shows how aggressively the system is currently learning. A higher value means it is reacting to a larger temperature error, while a value near zero means the room temperature is stable and close to the target.

---

## Advanced Control: Using the Proactive Bias

During operation, you can use the `Auto-Adaptive: Setpoint Bias` slider to proactively influence the controller. This allows you to create automations in Home Assistant that anticipate future events.

* **Example 1: Pre-heating for cheap energy**: Set a positive bias (e.g., `+1.0°C`) to pre-heat the house during off-peak electricity hours, using your floor as a thermal battery.
* **Example 2: Anticipating sunshine**: Set a negative bias (e.g., `-0.7°C`) to reduce heating on a cold but sunny morning, letting the "free" solar gain do the work instead of the heat pump.