# Auto-Adaptive Control

The **Auto-Adaptive Control** feature transforms your Ecodan heat pump into a smart, self-steering system. It intelligently adjusts the heat pump's operation to match the unique thermal properties of your home, maximizing both comfort and energy efficiency.

Instead of a static heating curve (based on weather), this controller uses a **load-compensated** strategy. It calculates the flow temperature based on the *real-time heat demand* of the house.

---

## Heating Strategy: Delta-T Control

This is the core of the Auto-Adaptive controller. It calculates the target flow temperature based on the **real-time heat demand** of your house, which it measures using the `return_temp` sensor.

### How It Works

The strategy follows these steps:

1.  **Select Profile**: It uses the `heating_system_type` setting to select one of six profiles. This choice determines three key variables:
    * `min_delta_t` / `max_delta_t`: The stable operating range.
    * `max_error_range`: The "sensitivity" of the controller.
2.  **Calculate Error**: It calculates the `error` (deviation from the setpoint). It automatically includes your **`setpoint_bias`**.
3.  **Scale Error (Smoothstep vs. Linear)**: This is the most important step. Based on your profile choice (with or without a `*`), the error is scaled in one of two ways:
    * **Gentle (Smoothstep):** Uses an S-curve formula. This is gentle at small errors (preventing overshoot) but accelerates quickly in the mid-range to provide power, before gently easing into the maximum. This is ideal for high-inertia systems like Underfloor Heating (UFH).
    * **Responsive (Linear):** `error_factor = error`. This reacts proportionally and is ideal for fast, low-inertia systems like Radiators or Hybrid systems where a quicker comfort response is desired.
4.  **Calculate Target ΔT**: It uses this `error_factor` to interpolate between the profile's (dynamically calculated) `min_delta_t` and `max_delta_t`.
    `target_delta_t = dynamic_min_delta_t + (error_factor * (max_delta_t - dynamic_min_delta_t))`
5.  **Calculate Target Flow**: The final target flow is calculated based on the real-time return temperature.
    `Calculated Flow = return_temp + target_delta_t`
6.  **Apply Boost (if active)**: The controller checks if the **Predictive Short-Cycle Prevention** is active. If it is, the calculated boost (e.g., +0.5°C) is added to the `Calculated Flow` *before* limits are applied.
7.  **Apply Limits & Rounding**: The final value is capped by your `minimum_heating_flow_temp` and `maximum_heating_flow_temp` settings. It is then rounded **down** (floored) to the nearest 0.5°C to ensure stable operation.

### Understanding the Profiles (Simulator)

You can use the **[Auto-Adaptive Simulator](https://gekkekoe.github.io/esphome-ecodan-hp/auto-adaptive-sim.html)** to interactively test the settings.

The simulator allows you to change all inputs (Return Temp, Room Temp, etc.) and instantly see how the algorithm will calculate the final flow temperature using both the **Linear** and **Smoothstep** curves.

### How to Choose Your Profile

The `Heating System Type` selector gives you six options, letting you choose both your physical system type and its "aggressiveness".

* **UFH (Gentle / Smoothstep):** For standard underfloor heating. Uses the gentle S-curve to maximize stability and prevent overshoot, while still being responsive during heat-up. **This is the recommended default.**
* **UFH \* (Responsive / Linear):** For UFH systems that feel too slow to respond. Uses the more aggressive `linear` curve.

* **Hybrid (Gentle / Smoothstep):** For mixed systems. Treats the system gently, like UFH, to prevent the fast radiators from causing overshoot.
* **Hybrid \* (Responsive / Linear):** For mixed systems where you want to prioritize the *fast comfort* of the radiators. This is a more aggressive, responsive setting.

* **Radiators (Gentle / Smoothstep):** For radiator-only systems with high thermal mass (e.g., old cast-iron radiators) where a gentle S-curve approach is still desired.
* **Radiators \* (Responsive / Linear):** For standard, low-inertia radiators. This profile is the most responsive to changes in temperature.

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

## Predictive Short-Cycle Prevention

This feature predicts imminent short cycles that can occur when the home's heat demand is lower than the heat pump's minimum power output.

When it detects a high-risk situation (actual flow temperature rising too far (>= 1.5c) above the requested flow), it proactively applies a **+0.5°C boost**. The high-risk condition is controlled by `High Delta Duration` (the minimum high-risk duration before the systems is applying the boost) and `High Delta Threshold` (the temp difference the system should monitor).

* **In Auto-Adaptive Mode:** This boost is stored in the `predictive_short_cycle_total_adjusted` variable. The main `auto_adaptive_loop` sees this boost and adds it to its own calculation. The loop is also responsible for resetting the boost to 0 when the risk is gone.
* **In Standalone Mode:** The boost is applied directly to the flow setpoint and added to the `predictive_short_cycle_total_adjusted` variable. It remains active for the entire compressor cycle and is reset by the `on_compressor_stop` script.

---

## Smart Boost Logic

The Auto-Adaptive algorithm includes a "Smart Boost" feature designed to eliminate steady-state errors. In situations where the room temperature stabilizes slightly below the setpoint (stagnation) and fails to bridge the final gap, this logic automatically increases the calculated heat output.

### How it works

The system continuously monitors the temperature error (`Target` - `Current`). If the error exceeds 0.1°C and does not decrease compared to the previous measurement, the system identifies this as stagnation.

After a predefined wait time (depending on the system type), a boost multiplier is applied to the calculated Delta T. If the stagnation persists, this multiplier increases in steps over time. This effectively increases the flow temperature to force the room temperature to the setpoint.

**Reset Condition:**
To prevent temperature overshoot, the boost factor is immediately reset to 1.0 (disabled) as soon as the temperature error decreases (i.e., the room temperature starts rising).

### System Profiles

The timing and aggressiveness of the Smart Boost logic are automatically configured based on the selected **Heating System Type**. This accounts for the significant difference in thermal mass and response time between floor heating (concrete mass) and radiators.

| System Type | Initial Wait Time | Step Interval | Max Boost Limit | Behavior |
| :--- | :--- | :--- | :--- | :--- |
| **Floor Heating** | 60 minutes | 30 minutes | +50% (1.5x) | Conservative timing to allow for slow thermal transfer in concrete floors. |
| **Radiators** | 20 minutes | 10 minutes | +150% (2.5x) | Faster reaction times suited for low-mass, high-temperature systems. |
| **Hybrid/Other** | 45 minutes | 20 minutes | +100% (2.0x) | Balanced approach for mixed systems. |



---

## Configuration Parameters

All parameters are adjustable in real-time from the Home Assistant interface.

| Parameter | Description | Guidance & Default |
| :--- | :--- | :--- |
| **`Auto-Adaptive: Control`** | **Enables or disables the entire Auto-Adaptive feature.** When disabled, the system will revert to using the standard fixed flow temperature setpoints. | **Default**: `On` |
| **`Auto-Adaptive: Heating System Type`** | Tunes the algorithm's behavior and responsiveness. Options marked with `*` are **Linear (Responsive)**. Others are **Quadratic (Gentle)**. | **Default**: `UFH (Gentle / Quadratic)`<br>• **UFH / UFH***: For underfloor heating.<br>• **Hybrid / Hybrid***: For mixed systems.<br>• **Radiators / Radiators***: For radiator-only systems. |
| **`Auto-Adaptive: Max. Heating Flow Temperature`**| Sets a hard safety limit for the flow temperature during heating to protect floors. | **Default**: `38.0°C` |
| **`Auto-Adaptive: Min. Heating Flow Temperature`**| Sets a hard stability limit for the flow temperature. Set this to the lowest stable temperature your heat pump can run at. | **Default**: `25.0°C` |
| **`Auto-Adaptive: Min. Cooling Flow Temperature`**| Sets a hard safety limit for the flow temperature during cooling to prevent condensation. | **Default**: `18.0°C` |
| **`Auto-Adaptive: Cooling Smart Start Temp`** | Sets an "efficiency ceiling" for cooling. The system will never start cooling with a flow temperature *higher* than this value. | **Default**: `19.0°C`<br>Must be ≥ `Min. Cooling Flow Temperature`. |
| **`Auto-Adaptive: Setpoint Bias`** | Applies a temporary adjustment to the target temperature for proactive control. | **Default**: `0.0°C`<br>This is the main "throttle" for external control (e.g., dynamic energy prices). |
| **`Auto-Adaptive: Room Temperature source`** | Selects the source for the **current** room temperature. | **Default**: `Room Thermostat`<br>• **Room Thermostat**: Uses the temperature from the Ecodan thermostat.<br>• **Home Assistant / REST API**: Overrides the temperature with an external sensor via the `temperature_feedback_z1` / `z2` helpers. |

---

# Getting Started with Auto-Adaptive Control

## Configuration Steps

### Step 1: Physical Thermostat Connection (If Applicable)

This guide assumes a common scenario where an external room thermostat (like a Tado, Nest, etc.) is wired to the **IN1** terminal on your heat pump's FTC board. This thermostat is responsible for sending the main heat request signal. It should also work with the remote wireless thermostat.

### Step 2: Set Up the Temperature Feedback Loop

The Auto-Adaptive algorithm needs to know the **current temperature** being measured by your external thermostat. You must send this value back to the ESPHome controller.

**If you are using a Mitsubishi wireless thermostat, MRC or [esphome-ecodan-remote-thermostat](https://github.com/gekkekoe/esphome-ecodan-remote-thermostat), you can skip the rest of this section and step 3.**

For systems with only a MRC (and no other thermostats), you will need to add a stop condition. The heatpump will never stop otherwise. Use server control prohibit to turn off heating when setpoint has been reached.

There are two options to perform **current temperature** feedback:

* **Via Home Assistant UI**: Create an automation in Home Assistant that copies the temperature from your thermostat's sensor to the `number.ecodan_heatpump_temperature_feedback_z1` entity.
* **Via REST API**: Use an external system or script to post the temperature directly.

Example REST API call to set the feedback temperature to 21.5°C:
```bash
curl -X POST "http://<esp_ip>/number/temperature_feedback_z1/set?value=21.5" -d ""
curl -X POST "http://<esp_ip>/number/temperature_feedback_z2/set?value=22.0" -d "" # only for 2 zones
```
Example Home Assistent automation:
```yaml
- id: SyncTemperatureToAdaptiveController
  alias: Sync Room Temp to Auto-Adaptive Controller
  description: "Ensures the heat pump's feedback value matches the kantoor current temp"
  trigger:
    - platform: template
      value_template: >-
        "{{ state_attr('climate.kantoor', 'current_temperature') | float(0) !=
          states('number.ecodan_heatpump_auto_adaptive_current_room_temperature_feedback_z1') | float(0) }}"
  condition: []
  action:
    - service: number.set_value
      target:
        entity_id: >-
          number.ecodan_heatpump_auto_adaptive_current_room_temperature_feedback_z1
      data:
        value: "{{ state_attr('climate.kantoor', 'current_temperature') | float(0) }}"
  mode: single
```

Put the automation in a file `automations.yaml` and include that file in the main home assistant `configuration.yaml`.
```yaml
automation: !include automations.yaml
```
Restart HA, and the automation should be visible in Settings > Automations & scenes. 

### Step 3: Set target temperatures (setpoint)
If you are using an external thermostat, you need to adjust the Zone 1 and Zone 2 temperature climate entities to reflect the setpoint of that external thermostat. This probably does not change often and can be done manually once. If it does change often, you can use an automation similar to the one in the previous step to sync the setpoint.

```yaml
- id: SyncSetpointToAdaptiveController
  alias: Sync Room Setpoint to Auto-Adaptive Controller
  description: Ensures the heat pump setpoint always matches the main setpoint
  trigger:
    - platform: template
      value_template: >-
        "{{ state_attr('climate.kantoor', 'temperature') | float(0) | round(1) !=
          state_attr('climate.ecodan_heatpump_zone_1_room_temp', 'temperature') | float(0) | round(1) }"
  condition: []
  action:
    - service: climate.set_temperature 
      target:
        entity_id: climate.ecodan_heatpump_zone_1_room_temp
      data:
        temperature: "{{ state_attr('climate.kantoor', 'temperature') | float(0) | round(1) }}"
  mode: single
        #temperature: "{{ (state_attr('climate.kantoor', 'temperature') * 2) | round(0) / 2.0 | float }}" if rounding to nearest half is needed
```
### Step 4: Configure Initial Parameters

In Home Assistant, navigate to your dashboard and set the initial parameters for the controller.

1.  **Set the Heating System Profile**: Choose the option from the `Auto-Adaptive: Heating System Type` dropdown that best matches your home (e.g., `UFH (Gentle / Quadratic)`). See the section "How to Choose Your Profile" above.
2.  **Set Safety Limits**: Adjust the `Auto-Adaptive: Max. Heating Flow Temperature` slider to a value that is safe for your floors (e.g., `38.0°C` for UFH). Do the same for the cooling limits.

### Step 5: Activate the System

Now you are ready to let the algorithm take control.

1.  **Set Heat Pump to Fixed Flow Mode**
2.  **Enable Auto-Adaptive Control**: In Home Assistant, turn **ON** the `Auto-Adaptive: Control` switch.

