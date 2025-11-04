## Heating Strategy: Delta-T (ΔT) Control

This is an advanced, alternative control strategy that you can select instead of the default `Self-Learning Curve`.

The key difference is that this strategy is **load-compensated** (reactive) rather than **weather-compensated** (predictive). It calculates the target flow temperature based on the **real-time heat demand** of your house, which it measures using the `return_temp` sensor.

### The Benefits

* **No Offset Drift:** This strategy does **not** use the learned `heating_curve_offset` global. The target flow temperature is calculated from scratch every cycle. This has a major advantage: the offset **cannot drift** to an extremely high or low value due to prolonged solar gain, misconfiguration, or unusual demand.
* **Automatic Gentle Start:** This strategy inherently provides a "slow morning start." Because the `return_temp` is low when starting, the calculated flow temperature will also be low, regardless of the `error`. The target temperature will then automatically "ramp up" as the floor (or radiator) returns warmer water to the heat pump.

### How It Works

The strategy follows these steps:

1.  **Select Profile:** It uses the `heating_system_type` setting to select one of three hard-coded ΔT profiles (UFH, Hybrid, or Radiators).
2.  **Calculate Error:** It calculates the `error` exactly as the main algorithm does. This is the crucial part: it automatically includes your **`setpoint_bias`** and **`thermostat_overshoot_compensation`**.
3.  **Scale Error (Non-Linear):** The `error` is clamped between `0.0` (no error) and `1.0` (large error). It is then scaled **non-linearly** (`error_factor = error * error`). This makes the controller very gentle at small errors but increasingly aggressive as the error grows.
4.  **Calculate Target ΔT:** It uses this `error_factor` to interpolate between the profile's `min_delta_t` (for efficiency) and `max_delta_t` (for power).
    `target_delta_t = min_delta_t + (error_factor * (max_delta_t - min_delta_t))`
5.  **Calculate Target Flow:** The final target flow is calculated based on the real-time return temperature.
    `Calculated Flow = return_temp + target_delta_t`
6.  **Rounding (Conservative):** The final value is rounded **down** (floored) to the nearest 0.5°C to match the heat pump's resolution. This ensures the controller is never more aggressive than intended, which provides a more stable and gentle ramp-up.
    `Final Flow = floor(Calculated Flow * 2) / 2.0`

### Getting started
- Set min/max flow temperature
- Select the correct heating system system. Look at the tables below to see what works best for your system.
- Select Delta T as heating strategy

That's it, it should tune the feed temp based on the error and delta T profile.

### Example Calculations

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