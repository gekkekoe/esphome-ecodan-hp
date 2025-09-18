# Short-Cycle Mitigation

The short-cycle mitigation feature protects your heat pump from excessive on/off cycling, which can reduce efficiency and stress the compressor.

When the compressor runtime is shorter than a predefined minimum on-time, the system automatically enters a **lockout phase** for a configurable duration. During this lockout phase:

- Heating and cooling outputs are temporarily disabled via server control.
- Domestic hot water (DHW) operation is **not affected**.

After the lockout duration expires, normal operation is automatically resumed, and the system restores its previous state.

## Key Features

- **Automatic protection:** Prevents short-cycling of the compressor.
- **Configurable parameters:** Minimum compressor on-time and lockout duration can be adjusted.
- **Manual override:** A cancel button allows the user to end a lockout phase early if needed.
- **System-awareness:** Only applies during heating or cooling; DHW operation is unaffected.

This feature improves the reliability, efficiency, and lifespan of your heat pump by ensuring that the compressor runs for a reasonable minimum period before cycling off.

## Parameters
| Parameter | Notes |
|:---|:---|
| Short-Cycle: Lockout | A binary sensor that indicates whether the system is currently in lockout mode. |
| Short-Cycle: Minimum On-Time | The minimum compressor on-time. If the compressor runtime is below this value, a lockout phase is initiated. |
| Short-Cycle: Lockout Duration (min) | The duration of the lockout. A value of 0 is the default and effectively disables this feature. |
| Short-Cycle: Cancel Lockout | A button to manually cancel a lockout phase. The lockout phase is automatically cleared after the specified lockout duration. |
