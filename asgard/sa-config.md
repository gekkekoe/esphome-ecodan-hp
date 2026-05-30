## Standalone Auto Adaptive Installation Guide (Asgard only)

The local dashboard is accessible at `http://ecodan-heatpump.local/dashboard` (or `http://<asgard-ip>/dashboard`).
![Standalone config](img/sa-config.png?v=4) 
The dashboard has three main tabs: **Monitor**, **Settings**, and **Logs**. A fourth **Solver** tab is hidden by default and must be enabled in Advanced Control (see [Enabling the Solver Tab](#enabling-the-solver-tab-odin-integration)).

---

## Dashboard Overview

### Monitor Tab

Live charts showing heat pump status, flow/return temperatures, compressor frequency, and outdoor temperature. A **Performance** section displays current heating COP, cooling COP, and DHW COP. Use the chart selector at the top to switch between views. Chart data is stored for 7 days; zoom and scroll back to review the last full day.

### Logs Tab

Live scrolling log of all Asgard activity. Controls:
- **Pause** — freezes the display without stopping logging
- **Clear** — clears the log buffer
- **Download** — saves the current log as a text file

---

## Manual Configuration of Auto Adaptive Mode

### Step 1 — Set Operating Mode

Open the **Settings** tab. Under **Zone 1**, set **Operating Mode** to `Heat Flow Temperature`.

For 2-zone systems, repeat for **Zone 2**.

> For cooling, select `Cool Flow Temperature` or `Cool Target Temperature` (Mitsubishi native AA, Asgard AA setting does not apply) instead. The AA settings panel automatically adjusts to show cooling-specific controls when a cooling mode is active.

---

### Step 2 — Configure the Virtual Thermostat

The Virtual (Software) Thermostat is the relay-driven room control that allows Asgard to directly signal the heat pump to start or stop via the R1/R2 relay inputs.

Under **Zone 1**, toggle **Use Room Thermostat Z1** to **off**. The **Virtual Thermostat Z1** configuration panel appears.

Set the **Thermostat Mode** to match your current operating mode if it is mismatching:

| Mode | Use when |
|------|---------|
| **Heat** | Heating season |
| **Cool** | Cooling season |
| **Off** | Manually disable the zone |

Set **Temp Sensor Source** to the source that is providing the virtual thermostat temperature:

| Option | Description |
|--------|-------------|
| **Virtual Thermostat Input** | A sensor feeding temperature via REST API (Home Assistant automation, Node-RED, etc.) |
| **DS18x20** | A wired Dallas DS18B20 sensor connected to the One Wire header on the Asgard PCB |
| **MRC** | The temperature sensor inside the main Mitsubishi display unit. Low resolution (0.5°C steps) — use only if no other sensor is available |

The current temperature and a setpoint stepper are shown below the selector. 

For 2-zone systems, repeat for **Zone 2**.

---

### Step 3 — Configure Auto Adaptive Settings

Scroll down to the **Auto Adaptive** section in the Settings tab.

#### General Settings

| Control | Description |
|---------|-------------|
| **Enable AA Control** | Master switch. Must be on for the algorithm to run. |
| **Defrost Mitigation** | Reduces flow temperature in the 30 minutes before a predicted defrost cycle to limit the temperature drop during defrost. |
| **Smart Boost** | Temporarily increases flow temperature setpoint when the house is recovering from a large temperature deficit, speeding up recovery without long overshoots. |

#### General Zone Settings

| Control | Description |
|---------|-------------|
| **Heating System Type** | Tells the algorithm the thermal inertia of your emitter system. Options with `*` are variants with modified (linear) response curves. Choose the closest match. |
| **Setpoint Bias** | A fixed offset (±5°C, 0.1°C steps) added to the AA-calculated flow temperature. Use to fine-tune if the system runs consistently too cool or too warm. |
| **Global Smart Start Temp** | The upperbound temperature for cooling. Cool flow temp is always in between [min_cool_flow, smart_start] Applies globally to all zones. |

#### Settings Z1 / Z2

| Control | Description |
|---------|-------------|
| **Room Temp Source** | Where AA reads the room temperature from. See table below. |
| **Max Flow Z1/Z2** | The maximum flow temperature AA is allowed to request. Keep this below the absolute system limit configured in the heat pump. |
| **Min Flow Z1/Z2** | The minimum flow temperature AA will request. Prevents the heat pump short-cycling because the calculated setpoint is too low to sustain operation. |
| **Min Cool Flow Z1/Z2** | *(Cooling mode only)* Minimum chilled water flow temperature for cooling. Shown only when a cooling operating mode is active. |

**Room Temp Source options (for AA):**

| Option | Description |
|--------|-------------|
| **Room Thermostat** | Choose this option if you are using the MRC or Wireless thermostat. |
| **Home Assistant / REST API** | Reads room temperature from a value pushed via REST API. See [Configure via REST API](#configure-virtual-thermostat-room-temperature-via-rest-api) below. Use this option when you don't have an Asgard PCB installed or did not wired R1/R2.|
| **Asgard Virtual Thermostat** | Uses the Virtual Thermostat relay (R1/R2) as the control signal and the room temperature source. Requires R1/R2 to be wired. The virtual thermostat current room temperature is used for AA calculations. |

#### Short Cycle Protection *(heating mode only)*

| Control | Description |
|---------|-------------|
| **Predictive Control** | Monitor and predict if a short cycle is imminent and increase flow setpoint to enable a longer run. |
| **Time Window (min)** | Look-ahead window for the predictive algorithm (1.0–5.0 min, 0.5 steps). |
| **Delta Threshold (°C)** | How many degrees before setpoint the predictive ramp-down begins (1.0–3.0°C, 0.5 steps). |

Click **Apply Settings** to save all Auto Adaptive settings.

---

### Step 4 — [Optional] Apply Short Cycle Lockout

Found under **Advanced Control → Short Cycle Lockout**: (Recommended when cooling)

| Control | Description |
|---------|-------------|
| **Lockout Status** | Shows whether a lockout is currently active and the remaining duration. Can be manually cancelled. |
| **Lockout Duration** | How long to block a restart after the compressor stops: 0, 15, 30, 45, or 60 minutes. |
| **Minimum On-Time (min)** | The minimum time the compressor must run before it is allowed to stop again (1–20 min). Prevents very short heating cycles that wear the compressor. |

---

## Configure Virtual Thermostat Room Temperature via REST API

Asgard exposes a REST API for reading and writing the virtual thermostat input temperature. Any sensor that can make HTTP calls can feed room temperature this way — Home Assistant, Node-RED, a Python script, etc. If you have already used the blueprint to link the temperature sensor, you can skip this section.

**Entities:**
- Zone 1: `asgard_vt_input_z1`
- Zone 2: `asgard_vt_input_z2`

### Get current value (Zone 1)
```bash
curl -X GET "http://ecodan-heatpump.local/number/Virtual%20Thermostat%20Input%20z1"
# Returns:
{"name_id":"number/Virtual Thermostat Input z1","id":"number-virtual_thermostat_input_z1","value":"22.0","state":"22.0 °C"}
```

### Set temperature to 21.5°C (Zone 1)
```bash
curl -X POST "http://ecodan-heatpump.local/number/Virtual%20Thermostat%20Input%20z1/set?value=21.5" -d ""
```

---

## Enabling the Solver Tab (ODIN Integration)

ODIN integration is configured in a dedicated **Solver** tab that is hidden by default to keep the standard interface clean.

To show it: go to **Settings → Advanced Control → Odin Solver** and enable **Show Solver Tab**. The Solver tab will appear in the navigation bar.

