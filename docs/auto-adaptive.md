# Auto-Adaptive Control

The **Auto-Adaptive Control** feature transforms your Ecodan heat pump into a smart, self-steering system. It intelligently adjusts the heat pump's operation to match the unique thermal properties of your home, maximizing both comfort and energy efficiency.

Instead of a static heating curve (based on weather), this controller uses a **load-compensated** strategy. It calculates the flow temperature based on the *real-time heat demand* of the house.

## Heating Strategy: Delta-T Control

This is the core of the Auto-Adaptive controller. It calculates the target flow temperature based on the **real-time heat demand** of your house, which it measures using the `return_temp` sensor.

### How It Works

The strategy follows these steps:

1.  **Select Profile**: It uses the `heating_system_type` setting to select one of three hard-coded ΔT profiles (UFH, Hybrid, or Radiators).
2.  **Calculate Error**: It calculates the `error` (deviation from the setpoint). This is crucial: it automatically includes your **`setpoint_bias`** and **`thermostat_overshoot_compensation`**.
3.  **Scale Error (Non-Linear)**: The `error` is clamped between `0.0` (no error) and `1.0` (large error). It is then scaled **non-linearly** (`error_factor = error * error`). This makes the controller very gentle at small errors but increasingly aggressive as the error grows.
4.  **Calculate Target ΔT**: It uses this `error_factor` to interpolate between the profile's `min_delta_t` (for efficiency) and `max_delta_t` (for power).
    `target_delta_t = min_delta_t + (error_factor * (max_delta_t - min_delta_t))`
5.  **Calculate Target Flow**: The final target flow is calculated based on the real-time return temperature.
    `Calculated Flow = return_temp + target_delta_t`
6.  **Rounding (Conservative)**: The final value is rounded **down** (floored) to the nearest 0.5°C to match the heat pump's resolution. This ensures the controller is never more aggressive than intended, which provides a more stable and gentle ramp-up.
    `Final Flow = floor(Calculated Flow * 2) / 2.0`

### Example calculated flow temperature for 22c return temperature

These tables show what the final target flow temperature will be based on the system type and the `error`.

#### Profile 1: UFH (Underfloor Heating)
* **Return Temp:** 22.0°C
* **Min ΔT:** 2.5°C
* **Max ΔT:** 6.0°C

| Error | Error Factor (error^2) | Target ΔT | Calculated Flow | Final Flow (Floored) |
| :--- | :--- | :--- | :--- | :--- |
| 0.0 | 0.00 | 2.50°C | 24.50°C | **24.5°C** |
| 0.1 | 0.01 | 2.54°C | 24.54°C | **24.5°C** |
| 0.2 | 0.04 | 2.64°C | 24.64°C | **24.5°C** |
| 0.3 | 0.09 | 2.82°C | 24.82°C | **24.5°C** |
| 0.4 | 0.16 | 3.06°C | 25.06°C | **25.0°C** |
| 0.5 | 0.25 | 3.38°C | 25.38°C | **25.0°C** |
| 0.6 | 0.36 | 3.76°C | 25.76°C | **25.5°C** |
| 0.7 | 0.49 | 4.22°C | 26.22°C | **26.0°C** |
| 0.8 | 0.64 | 4.74°C | 26.74°C | **26.5°C** |
| 0.9 | 0.81 | 5.34°C | 27.34°C | **27.0°C** |
| 1.0 | 1.00 | 6.00°C | 28.00°C | **28.0°C** |

#### Profile 2: Hybrid (UFH + Radiators)
* **Return Temp:** 22.0°C
* **Min ΔT:** 4.0°C
* **Max ΔT:** 8.0°C

| Error | Error Factor (error^2) | Target ΔT | Calculated Flow | Final Flow (Floored) |
| :--- | :--- | :--- | :--- | :--- |
| 0.0 | 0.00 | 4.00°C | 26.00°C | **26.0°C** |
| 0.1 | 0.01 | 4.04°C | 26.04°C | **26.0°C** |
| 0.2 | 0.04 | 4.16°C | 26.16°C | **26.0°C** |
| 0.3 | 0.09 | 4.36°C | 26.36°C | **26.0°C** |
| 0.4 | 0.16 | 4.64°C | 26.64°C | **26.5°C** |
| 0.5 | 0.25 | 5.00°C | 27.00°C | **27.0°C** |
| 0.6 | 0.36 | 5.44°C | 27.44°C | **27.0°C** |
| 0.7 | 0.49 | 5.96°C | 27.96°C | **27.5°C** |
| 0.8 | 0.64 | 6.56°C | 28.56°C | **28.5°C** |
| 0.9 | 0.81 | 7.24°C | 29.24°C | **29.0°C** |
| 1.0 | 1.00 | 8.00°C | 30.00°C | **30.0°C** |

#### Profile 3: Radiators
* **Return Temp:** 22.0°C
* **Min ΔT:** 5.0°C
* **Max ΔT:** 10.0°C

| Error | Error Factor (error^2) | Target ΔT | Calculated Flow | Final Flow (Floored) |
| :--- | :--- | :--- | :--- | :--- |
| 0.0 | 0.00 | 5.00°C | 27.00°C | **27.0°C** |
| 0.1 | 0.01 | 5.05°C | 27.05°C | **27.0°C** |
| 0.2 | 0.04 | 5.20°C | 27.20°C | **27.0°C** |
| 0.3 | 0.09 | 5.45°C | 27.45°C | **27.0°C** |
| 0.4 | 0.16 | 5.80°C | 27.80°C | **27.5°C** |
| 0.5 | 0.25 | 6.25°C | 28.25°C | **28.0°C** |
| 0.6 | 0.36 | 6.80°C | 28.80°C | **28.5°C** |
| 0.7 | 0.49 | 7.45°C | 29.45°C | **29.0°C** |
| 0.8 | 0.64 | 8.20°C | 30.20°C | **30.0°C** |
| 0.9 | 0.81 | 9.05°C | 31.05°C | **31.0°C** |
| 1.0 | 1.00 | 10.00°C | 32.00°C | **32.0°C** |

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

To mitigate this, the algorithm proactively adds a `+0.5°C` boost to the learned curve offset, forcing a higher feed temp. This action may cause a minor room temperature overshoot, which is then automatically corrected by the main auto-adaptive algorithm in subsequent learning cycles.

---

## Configuration Parameters

All parameters are adjustable in real-time from the Home Assistant interface.

| Parameter                                     | Description                                                                          | Guidance & Default                                                                                                                              |
| --------------------------------------------- | ------------------------------------------------------------------------------------ | ----------------------------------------------------------------------------------------------------------------------------------------------- |
| **`Auto-Adaptive: Control`** | **Enables or disables the entire Auto-Adaptive feature.** When disabled, the system will revert to using the standard fixed flow temperature setpoints. | **Default**: `On`                                                                                                                               |
| **`Auto-Adaptive: Heating System Type`** | Tunes the algorithm's behavior to match your system's thermal inertia (response time). | **Default**: `Underfloor Heating`<br>• **UFH**: For slow, high-inertia systems.<br>• **UFH + Radiators**: For hybrid systems.<br>• **Radiators**: For fast, low-inertia systems. |
| **`Auto-Adaptive: Max. Heating Flow Temperature`**| Sets a hard safety limit for the flow temperature during heating to protect floors.      | **Default**: `38.0°C`                                                                                                                           |
| **`Auto-Adaptive: Min. Cooling Flow Temperature`**| Sets a hard safety limit for the flow temperature during cooling to prevent condensation. | **Default**: `18.0°C`                                                                                                                           |
| **`Auto-Adaptive: Cooling Smart Start Temp`** | Sets an "efficiency ceiling" for cooling. The system will never start cooling with a flow temperature *higher* than this value, preventing inefficient cycles on mild days. | **Default**: `19.0°C`<br>Must be ≥ `Min. Cooling Flow Temperature`. |
| **`Auto-Adaptive: Thermostat Overshoot Compensation`** | Tells the algorithm to ignore a certain amount of overshoot caused by an external thermostat's own hysteresis. | **Default**: `0.0°C`<br>Set this to the known overshoot of your thermostat (e.g., `1.0°C`) to prevent incorrect learning. |
| **`Auto-Adaptive: Setpoint Bias`** | Applies a temporary adjustment to the target temperature for proactive control. | **Default**: `0.0°C`<br>A range of **-1.5°C to +1.5°C** is effective for UFH, while **-2.5°C to +2.5°C** can be used for radiators. |
| **`Auto-Adaptive: Room Temperature source`** | Selects the source for the **current** room temperature. The **target** temperature is always read from the active Ecodan thermostat. | **Default**: `Room Thermostat`<br>• **Room Thermostat**: Uses the current temperature from the Ecodan thermostat.<br>• **Rest API**: Overrides the current temperature with an external sensor. E.g.:<br>`curl -X POST "http://<esp_ip>/number/temperature_feedback/set?value=21.5"`|

---

# Getting Started with Auto-Adaptive Control

## Configuration Steps

### Step 1: Physical Thermostat Connection (If Applicable)

This guide assumes a common scenario where an external room thermostat (like a Tado, Nest, etc.) is wired to the **IN1** terminal on your heat pump's FTC board. This thermostat is responsible for sending the main heat request signal to the heat pump. It should work with the remote wireless thermostat as well.

### Step 2: Set Up the Temperature Feedback Loop

The Auto-Adaptive algorithm needs to know the **current temperature** being measured by your external thermostat. You must send this value back to the ESPHome controller. If you are using a mitsubishi wireless thermostat or [esphome-ecodan-remote-thermostat](https://github.com/gekkekoe/esphome-ecodan-remote-thermostat), you can skip the rest of this section and step 3.

You can do this in two ways:

* **Via Home Assistant UI**: Create an automation in Home Assistant that copies the temperature from your thermostat's sensor to the `Auto-Adaptive: Current Room Temperature Feedback` number entity.
* **Via REST API**: Use an external system or script to post the temperature directly.

Example REST API call to set the feedback temperature to 21.5°C:
```bash
curl -X POST "http://esp_ip/number/auto-adaptive__current_room_temperature_feedback_z1/set?value=21.5" -d ""
curl -X POST "http://esp_ip/number/auto-adaptive__current_room_temperature_feedback_z2/set?value=22.0" -d "" # only for 2 zones
```

Example Home Assistent automation:
```yaml
- id: SyncTemperatureToAdaptiveController
  alias: Sync Room Temp to Auto-Adaptive Controller
  description: "Ensures the heat pump's feedback value matches the kantoor current temp"
  trigger:
    - platform: template
      value_template: >-
        {{ state_attr('climate.kantoor', 'current_temperature') | float(0) !=
          states('number.ecodan_heatpump_auto_adaptive_current_room_temperature_feedback_z1') | float(0) }}
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
        {{ state_attr('climate.kantoor', 'temperature') | float(0) | round(1) !=
          state_attr('climate.ecodan_heatpump_zone_1_room_temp', 'temperature') | float(0) | round(1) }}
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

In Home Assistant, navigate to your dashboard and set the initial parameters for the controller on the **"Auto-Adaptive Settings"** tab.

1.  **Set the Heating System Profile**: Choose the option from the `Auto-Adaptive: Heating System Type` dropdown that best matches your home (`Underfloor Heating`, `Underfloor Heating + Radiators`, or `Radiators`).
2.  **Set Safety Limits**: Adjust the `Auto-Adaptive: Max. Heating Flow Temperature` slider to a value that is safe for your floors (e.g., `38.0°C` for UFH). Do the same for the cooling limits.
3.  **Set Thermostat Overshoot Compensation (optional)**: Adjust the `Auto-Adaptive: Thermostat Overshoot Compensation` slider to match the known behavior of your thermostat. For example, the official Mitsubishi wireless thermostat has an  overshoot of 1.0°C, so you would set this slider to 1.0.

### Step 5: Activate the System

Now you are ready to let the algorithm take control.

1.  **Set Heat Pump to Fixed Flow Mode**: On your main Mitsubishi thermostat or control panel, ensure that the active zone(s) are set to **Fixed Flow Temperature** mode for heating (and cooling, if applicable). This is critical, as it allows the ESPHome controller to have full control. Set a start fixed flow temperature (only needed for the first usage).
2.  **Enable Auto-Adaptive Control**: In Home Assistant, turn **ON** the `Auto-Adaptive: Control` switch.

---

## Advanced Control: Using the Proactive Bias

During operation, you can use the `Auto-Adaptive: Setpoint Bias` slider to proactively influence the controller. This allows you to create automations in Home Assistant that anticipate future events.

* **Example 1: Pre-heating for cheap energy**: Set a positive bias (e.g., `+1.0°C`) to pre-heat the house during off-peak electricity hours, using your floor as a thermal battery.
* **Example 2: Anticipating sunshine**: Set a negative bias (e.g., `-0.7°C`) to reduce heating on a cold but sunny morning, letting the "free" solar gain do the work instead of the heat pump.