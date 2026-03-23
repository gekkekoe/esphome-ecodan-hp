# ODIN Getting Started Guide

> **ODIN** — Dynamic Cost Optimizer for heat pumps. This guide walks you through first boot, Wi-Fi setup, firmware updates, and a complete description of every setting in the dashboard.

---

## Table of Contents

1. [What You Need](#1-what-you-need)
2. [First Boot — Wi-Fi Setup](#2-first-boot--wi-fi-setup)
3. [Navigating the Dashboard](#3-navigating-the-dashboard)
4. [Firmware Updates (OTA)](#4-firmware-updates-ota)
5. [Settings — Location & Energy](#5-settings--location--energy)
6. [Settings — Solver Tuning](#6-settings--solver-tuning)
7. [Settings — 24h Schedule Profile](#7-settings--24h-schedule-profile)
8. [Environmental Data Tab](#8-environmental-data-tab)
9. [Monitor Tab](#9-monitor-tab)
10. [Quick-Start Checklist](#10-quick-start-checklist)
11. [Asgard — Solver Tab](#11-asgard--solver-tab)
12. [Monitoring Performance — Room Temperature Charts](#12-monitoring-performance--room-temperature-charts)

---

## 1. What You Need

- Asgard Hardware with virtual thermostats wired
- ODIN hardware
- A browser to complete initial setup
- Your location coordinates (latitude / longitude)
- Basic knowledge of your house: heating system type, solar panel capacity (if any)

---

## 2. First Boot — Wi-Fi Setup

When ODIN boots for the first time (or after a Wi-Fi reset), it cannot connect to your home network yet. It creates its own temporary access point so you can configure it.

### Step 1 — Connect to the ODIN access point

On your phone or laptop, open Wi-Fi settings and look for a network named:

```
ODIN_Setup
```

Connect to it. No password is required.

### Step 2 — Open the dashboard

Open a browser and go to:

```
http://192.168.4.1
```

The ODIN dashboard will load.

### Step 3 — Enter your Wi-Fi credentials

1. Click the **Firmware** tab (last tab in the navigation bar)
2. Scroll to the **Wi-Fi Configuration** section
3. Enter your home network **SSID** (network name) and **Password**
4. Click **Save & Reboot**

ODIN will restart and connect to your home network. The access point disappears.

### Step 4 — Find ODIN on your network

After reboot, ODIN is accessible via its local IP address. You can find this in your router's connected devices list, or try:

```
ping odin.local
```

> **Tip:** Assign a static IP to ODIN in your router settings so the address never changes.

---

## 3. Navigating the Dashboard

The dashboard has four tabs at the top:

| Tab | Purpose |
|-----|---------|
| **Monitor** | Live system status, optimization logs, debug output |
| **Settings** | All configuration parameters |
| **Data** | Today's electricity prices and weather forecast |
| **Firmware** | Wi-Fi config and OTA firmware updates |

---

## 4. Firmware Updates (OTA)

ODIN supports over-the-air (OTA) firmware updates directly from the dashboard — no USB cable needed.

### How to update

1. Download the latest `.bin` firmware file from the ODIN releases page
2. Open the ODIN dashboard and go to the **Firmware** tab
3. Scroll to **Firmware Update (OTA)**
4. Click **Select .BIN** and choose the downloaded file
5. Click **Upload & Flash**
6. Wait — the upload takes 20–60 seconds depending on file size
7. ODIN will automatically reboot into the new firmware

> ⚠️ **Do not power off the device during the update.** 

The current firmware version is shown in the top-left corner of the dashboard header.

---

## 5. Settings — Location & Energy

These settings tell ODIN where you are, how to fetch electricity prices, and basic information about your solar panels.

Go to **Settings** tab to find these fields.

---

### Pricing Mode

**What it does:** Chooses whether ODIN uses real-time hourly spot prices or a fixed flat rate.

| Option | When to use |
|--------|------------|
| **Dynamic (Spot)** | You have a dynamic electricity contract where the price changes every hour. ODIN fetches today's prices automatically and plans heating around the cheapest hours. |
| **Fixed Rate** | You pay a flat rate per kWh. ODIN still optimises around comfort and solar, but all hours are treated as equal cost. |

---

### Fixed Price (€/kWh)

*Only shown when Pricing Mode is set to Fixed Rate.*

**What it does:** The flat electricity price you pay per kWh.

**Example:** If you pay €0.28/kWh, enter `0.280`.

---

### Price Zone

**What it does:** Selects your electricity market zone. This determines which market data source is used for spot prices, and which energy tax and VAT rates apply to your cost calculations.

Choose the zone that matches your country. If your country is not listed, choose the closest one or use Fixed Rate mode instead.

| Zone | Country |
|------|---------|
| NL | Netherlands |
| BE | Belgium |
| DE-LU | Germany / Luxembourg |
| FR | France |
| AT | Austria |
| CH | Switzerland |
| DK1 / DK2 | Denmark |
| SE4 | Sweden (South) |
| NO2 | Norway (South) |
| PL | Poland |
| HU | Hungary |
| CZ | Czech Republic |
| SI | Slovenia |
| IT-NORTH | Italy (North) |

---

### Price Source

**What it does:** Selects which data provider ODIN uses to fetch spot prices.

| Option | Description |
|--------|-------------|
| **Energy-Charts (default)** | Free, no account needed. Covers most European zones. Recommended for most users. |
| **ENTSO-E Transparency** | Official European energy exchange data. Requires a free API token. More reliable in some zones. |

---

### ENTSO-E Token

*Only shown when Price Source is set to ENTSO-E.*

**What it does:** Your personal API token from the ENTSO-E Transparency Platform.

To get a token: register for free at [transparency.entsoe.eu](https://transparency.entsoe.eu) and request a Web API security token in your account settings.

**Format:** `xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx`

---

### Latitude

**What it does:** Your geographic latitude. Used to fetch the correct weather forecast and solar irradiance data for your location.

**How to find it:** Search your address on [maps.google.com](https://maps.google.com), right-click on your location, and the coordinates appear at the top of the context menu.

**Format:** Decimal degrees, e.g. `52.3702` for Amsterdam.

---

### Longitude

**What it does:** Your geographic longitude. Used together with Latitude.

**Format:** Decimal degrees, e.g. `4.8952` for Amsterdam.

---

### Solar Capacity (kWp)

**What it does:** The peak output of your solar PV panels in kilowatts-peak. ODIN uses this to estimate how much of the heat pump's electricity is covered by your panels each hour.

During hours when your panels produce more electricity than the heat pump needs, the effective electricity cost drops to near zero — ODIN will prefer to run the heat pump during these hours.

**If you have no solar panels:** enter `0`.

**Examples by system size:**

| System | kWp |
|--------|-----|
| Small (6 panels) | 2.5 |
| Medium (12 panels) | 4.5 |
| Large (25 panels) | 10.0 |
| Very large (28 panels) | 11.0 |

---

### Solar Orientation

**What it does:** The compass direction your solar panels face. South-facing panels produce more electricity at midday. East or West-facing panels produce more in the morning or afternoon respectively. ODIN applies a correction factor based on orientation so the solar estimate is accurate.

| Orientation | Factor | Notes |
|-------------|--------|-------|
| South | 1.00 | Optimal |
| South-East / South-West | 0.95 | Slight reduction |
| East / West | 0.85 | Morning or afternoon bias |
| North-East / North-West | 0.70 | Significantly reduced |
| North | 0.60 | Minimal production |

Choose the direction that best matches your roof.

---

## 6. Settings — Solver Tuning

These parameters control how the ODIN optimisation engine balances cost savings against comfort. Most users only need to set the **kWh Penalty** and optionally the **Cheap Energy Threshold**.

---

### kWh Penalty (Cost Bias)

**What it does:** Controls how aggressively ODIN pre-heats during cheap hours versus keeping the house close to the target temperature at all times.

This is the most important tuning parameter. Think of it as a slider between "maximum savings" and "maximum comfort stability."

| Value | Behaviour |
|-------|-----------|
| `0.1` | Very aggressive. Large temperature swings. Maximum cost savings. |
| `0.3` | Recommended for well-insulated or passive houses with UFH. |
| `0.5` | Balanced. Good starting point for most houses. |
| `1.0` | Conservative. Pre-heats only when price difference is large. |
| `2.0` | Minimal optimisation. House stays close to target at all times. |

**Recommended starting values by house type:**

| House type | Recommended value |
|------------|-------------------|
| Passive / near-passive, UFH, high thermal mass | 0.3 |
| Modern well-insulated, UFH | 0.5 |
| Average house, radiators | 0.7 |
| Older draughty house | 1.0 |

> **Tip:** Start at `0.5` and reduce toward `0.3` if your house holds temperature well and you want bigger savings. Increase toward `1.0` if temperature swings feel uncomfortable.

---

### "Cheap" Energy Threshold (€/kWh)

**What it does:** Any hour with a spot price below this threshold is treated as a "cheap" hour. During cheap hours, ODIN applies a very low comfort penalty for running above the target temperature — effectively encouraging pre-heating whenever prices are very low, even if solar is not available.

**Unit:** €/kWh (not €/MWh). Enter the full price including tax if you want to compare to what you actually pay, or the raw spot price without tax.

**Examples:**

| Contract / market | Suggested threshold |
|-------------------|---------------------|
| NL dynamic contract, typical spread | `0.06` (6 cents/kWh) |
| BE / DE with cheap night tariff | `0.05` |
| Area with frequent negative prices | `0.02` |

**Default:** `0.04` (4 cents/kWh)

---

### Min. Solar Coverage (%)

**What it does:** The minimum fraction of the heat pump's electricity load that must be covered by your solar panels before ODIN allows low-penalty pre-heating during mid-priced hours. This prevents the heat pump from running at moderate prices just because it "might" be somewhat solar-assisted.

**Unit:** Percentage (0–200%). Values above 100% mean solar must cover more than the full HP load — this is rarely useful.

**Examples:**

| Setting | Meaning |
|---------|---------|
| `0%` | Solar coverage is ignored for the comfort penalty. Pre-heating is driven by price only. |
| `50%` | At least 50% of the HP electricity must come from solar before this hour is treated as "cheap". |
| `100%` | Solar must fully cover the HP before the low penalty applies. |

**Recommended:** `50` for most setups. If you have large panels relative to your HP size, lower this to `30`.

---

## 7. Settings — 24h Schedule Profile

The schedule defines your **comfort temperature band** for each hour of the day. ODIN will never plan to heat above the maximum or let the house cool below the minimum.

### How the schedule works

The schedule is divided into **time blocks**. Each block defines settings that apply from its start hour until the next block begins. The last block wraps around to midnight.

Each block has four fields:

| Field | Description |
|-------|-------------|
| **Hour** | The hour (0–23) at which this block starts |
| **Base** | Your target ("setpoint") temperature in °C |
| **Min** | Offset below Base that is still acceptable (e.g. `-0.5` means the house can be 0.5°C below Base) |
| **Max** | Offset above Base that is still acceptable (e.g. `+1.5` means the house can be 1.5°C above Base) |

The resulting comfort band is `[Base + Min, Base + Max]`. ODIN plans heating to stay within this band at all times.

### Example schedule

| Hour | Base | Min | Max | Meaning |
|------|------|-----|-----|---------|
| 0 | 21.0 | −0.5 | +1.5 | Night: 20.5–22.5°C acceptable |
| 7 | 21.5 | −0.5 | +1.5 | Morning: 21.0–23.0°C acceptable |
| 9 | 22.0 | −0.5 | +1.5 | Day: 21.5–23.5°C acceptable |
| 22 | 21.0 | −0.5 | +1.5 | Evening wind-down |

### Tips for setting the schedule

- **Wide bands give ODIN more freedom** to shift heating to cheap/solar hours. A ±1.5°C band is a good starting point.
- **UFH systems** can tolerate wider bands because the floor stores heat. Try Base ± 1.5°C.
- **Radiator systems** react faster and may prefer tighter bands: Base ± 0.5°C.
- **Night setback:** lower the Base by 0.5–1.0°C during sleeping hours if comfort allows. ODIN will plan accordingly.
- **Avoid setting Min = 0** (same as Base). This prevents ODIN from ever letting the house drift slightly cool during expensive hours, which reduces savings potential.

### Adding and editing blocks

- Click **+ Add Time Block** to add a new block
- Edit any field directly in the row
- Drag the **⋮** handle to reorder blocks
- Click **×** to remove a block
- Click **Compile & Send** to save all changes

---

## 8. Environmental Data Tab

The **Data** tab shows the current inputs ODIN is working with:

**Energy Prices** — today's hourly spot prices in EUR/MWh, shown as a bar chart. The date of the loaded prices is shown below the chart title. If prices show `0` for all hours, click **Fetch Latest Data** to manually trigger a refresh.

**Weather Forecast** — today's hourly outside temperature (°C) and solar irradiance (W/m²) for your location. The time of the last weather fetch is shown below the title.

Click **Fetch Latest Data** at the top right to manually refresh both prices and weather. ODIN automatically fetches new data each day at 23:30 for the following day.

---

## 9. Monitor Tab

The **Monitor** tab shows live system status:

| Field | Description |
|-------|-------------|
| **Uptime** | How long ODIN has been running since last reboot |
| **Free Heap** | Available memory on the ESP32. Healthy values are above 100 KB. |
| **DP Runs** | Number of optimisation solves completed since boot |
| **Last Speed** | How long the last solve took in milliseconds. Typically 20–100ms. |
| **Last Optimization Status** | Result of the most recent solve attempt |

**System Logs** shows a live scrolling log of ODIN's activity. Use **Debug JSON** to download the last request and response payload — useful for troubleshooting. Use **Clear Logs** to reset the log buffer. **Pause** freezes the log display without stopping logging.

---

## 10. Quick-Start Checklist

Use this checklist after first-time setup to confirm everything is configured correctly:

- [ ] ODIN is connected to your home Wi-Fi (dashboard loads via local IP)
- [ ] **Latitude** and **Longitude** are set to your location
- [ ] **Price Zone** matches your country
- [ ] **Pricing Mode** is set to Dynamic (if you have a dynamic contract)
- [ ] **Solar Capacity** is set (or `0` if no panels)
- [ ] **Solar Orientation** matches your roof direction
- [ ] **kWh Penalty** is set (start with `0.5`)
- [ ] **24h Schedule** has at least one block with sensible temperature targets
- [ ] Go to the **Data** tab and click **Fetch Latest Data** — verify prices and weather load correctly
- [ ] Go to the **Monitor** tab — after a minute, **Last Optimization Status** should show a successful solve

---

## 11. Asgard — Solver Tab

The **Solver** tab in the Asgard dashboard is where you connect Asgard to ODIN, monitor the optimisation results, and fine-tune the physics parameters that the solver uses to model your house.

> **To show the Solver tab:** go to **Settings → Auto Adaptive → Odin Solver** and enable **Show Solver Tab**. The tab will appear in the navigation bar.

---

### 11.1 Connecting to ODIN

#### Automatic detection

Asgard automatically tries to find ODIN on your local network using mDNS. If ODIN is reachable at `odin.local`, Asgard resolves the IP address and connects without any manual configuration. When the connection is established, the **Connection Status** field turns green and shows **ONLINE**.

This happens in the background every 60 seconds. If ODIN is on the same Wi-Fi network as Asgard, no further action is needed.

#### Manual IP entry

If mDNS does not work on your network (some routers block it), you can enter ODIN's IP address manually:

1. Find ODIN's IP address in your router's connected devices list
2. Go to the **Solver** tab
3. Enter the IP address in the **Odin IP Address** field (e.g. `192.168.1.42`)
4. Click **Save IP**

Once a valid IP is stored, the **Open ODIN** button appears — click it to open the ODIN dashboard directly in a new tab.

#### Enable Dynamic Cost Solver

Toggle **Enable Dynamic Cost Solver** to activate ODIN-driven heating control. When enabled, Asgard fetches a new optimisation plan from ODIN every hour and uses it to control the heat pump relay. When disabled, normal thermostat control applies.

---

### 11.2 Running an Optimisation Manually

The **Run Optimization** button triggers an immediate solve without waiting for the next scheduled hour. This is useful after changing settings or to verify the solver is working correctly.

**From hour** field (next to the button): by default, the solve starts from the current hour. You can override this to test what the solver would plan from a different starting hour (0–23). Leave it blank to use the current hour.

After a successful run, the **Last Run Result** panel updates with:

| Field | Description |
|-------|-------------|
| **Execution time** | How long the solve took in milliseconds. Typically 20–100ms. |
| **Heat loss** | The heat loss coefficient ODIN derived from your data (kW/K) |
| **Base COP** | Coefficient of Performance at 7°C reference temperature |
| **Thermal mass** | Thermal energy storage capacity of your house (kWh/K) |
| **Expected consumption** | Forecast HP electricity use for the rest of today (kWh) |
| **Expected production** | Forecast heat output for the rest of today (kWh) |
| **Expected Solar production** | Solar PV energy that will cover HP electricity use (kWh) |
| **Expected Total cost** | Forecast raw energy cost, no tax (€) |
| **Expected Total cost incl. tax** | Forecast consumer cost including energy tax and VAT (€) |

---

### 11.3 Physics Data — Sent to Solver

This section shows the values Asgard sends to ODIN with every solve request. These are derived automatically from your heat pump's real measured data over the past day. They represent your house's thermal behaviour.

You can override any value manually and click **Apply Physics Data** to force specific values for testing. Asgard will use your overridden values until the next automatic daily update.

---

#### Heat Produced (kWh)

**What it is:** Total heat energy your heat pump produced yesterday in heating mode (DHW excluded).

**How it's used:** Together with Elec Consumed, this calculates your actual COP.

**Typical values:**

| House type | Winter | Spring |
|------------|--------|--------|
| Small apartment | 15–25 kWh | 5–10 kWh |
| Average house | 30–60 kWh | 10–25 kWh |
| Large passive house | 20–40 kWh | 5–15 kWh |

---

#### Elec Consumed (kWh)

**What it is:** Total electricity your heat pump consumed yesterday in heating mode (DHW excluded).

**How it's used:** `COP = Heat Produced / Elec Consumed`. Higher is more efficient.

**Typical values:** 5–15 kWh/day depending on house size and outside temperature.

---

#### Runtime (Hours)

**What it is:** How many hours the heat pump ran in heating mode yesterday.

**How it's used:** Together with the daily temperature data, this helps derive the thermal mass of your house.

**Typical values:** 8–18 hours in winter, 2–8 hours in spring.

---

#### Avg Outside Temp (°C)

**What it is:** Average outdoor temperature yesterday.

**How it's used:** Used to calculate heat loss and COP correction. Fetched automatically from your heat pump's outdoor sensor.

---

#### Avg Room Temp (°C)

**What it is:** Average room temperature measured yesterday.

**How it's used:** Together with Avg Outside Temp, this gives the temperature difference that drives heat loss calculations.

**Typical values:** 20–22°C for most households.

---

#### Delta Room Temp (°C)

**What it is:** The difference between the highest and lowest room temperature measured yesterday (daily swing).

**How it's used:** Used to estimate thermal mass — a large swing means low thermal mass (house heats and cools quickly), a small swing means high thermal mass (temperature stays stable).

**Typical values by house type:**

| House type | Delta |
|------------|-------|
| Light timber frame, radiators | 1.5–3.0°C |
| Average brick house | 0.8–1.5°C |
| Heavy concrete, UFH | 0.2–0.6°C |
| Near-passive / passive house | 0.1–0.3°C |

---

#### Max Output (kW)

**What it is:** The maximum thermal output your heat pump can deliver, in kilowatts.

**How it's used:** The solver will never plan heat input above this value. Setting it too low prevents the solver from planning recovery after cold nights. Setting it too high may result in plans that the HP cannot physically execute.

**How to set it:** Use the rated output of your heat pump at the typical outdoor temperature. Check the technical datasheet.

**Examples:**

| HP model type | Typical max output |
|---------------|-------------------|
| Small (5–7 kW nameplate) | 6.0–7.0 kW |
| Medium (9–12 kW nameplate) | 8.0–11.0 kW |
| Large (14–16 kW nameplate) | 12.0–15.0 kW |

> **Tip:** **Max Output as a soft cap**: You can set Max Output lower than your heat pump's physical maximum to intentionally limit how hard it runs. For example, setting 7 kW on a 10 kW heat pump means ODIN will never plan more than 7 kW of heat input per hour. This is useful for reducing noise at night or spreading peak electricity load. The heat pump will simply run longer at lower intensity to achieve the same result.

---

#### HL × TM Product (Tau)

**What it is:** The thermal time constant of your house in hours. Tau = Thermal Mass / Heat Loss. It describes how slowly your house loses heat when the HP is off.

**How it's used:** A higher Tau means the house holds heat longer. ODIN uses this to predict how much the temperature will drop during HP-off hours.

**This value is learned automatically** from nighttime cooldown periods. You do not normally need to set it manually. 

**Typical values:**

| House type | Tau (hours) |
|------------|-------------|
| Light timber frame | 20–40 h |
| Average Dutch brick house | 50–80 h |
| Well-insulated modern house | 70–110 h |
| Near-passive / passive house, UFH | 80–150 h |

A Tau of 80 hours means: with the HP off, the house takes 80 hours to lose 63% of its temperature advantage over outside. This is excellent for pre-heating — warmth stored in the morning stays until the evening.

---

#### Passive Solar Factor (kWh/W/m²)

**What it is:** How much heat enters your house through windows per watt per square metre of solar irradiance. It represents sunlight warming the house directly through the glass.

**How it's used:** On sunny days, the solver accounts for this free heat gain when planning HP operation. If the sun will warm the house to the target temperature anyway, the HP does not need to run.

**This value is learned automatically** from HP-off periods on sunny days.

**Recommended starting values:**

| House type | Starting value |
|------------|---------------|
| No south-facing windows, or heavily shaded | 0.002 |
| Normal house, some south glass | 0.010 |
| Modern house, good south glazing | 0.015 |
| Large house, many large south windows | 0.020–0.030 |
| Near-passive with extensive south glazing | 0.025–0.040 |

**Valid range:** 0.001–0.050. Values above 0.05 are physically unrealistic for any residential building.

---

#### Battery SoC (kWh)

**What it is:** Current state of charge of your home battery in kWh.

**How it's used:** If you have a home battery, ODIN can recommend which hours to discharge it to cover heat pump electricity costs. This field should be updated automatically via a Home Assistant automation that reads your battery's SoC sensor.

**If you have no battery:** leave at `0`.

---

#### Battery Max Discharge (kW)

**What it is:** The maximum rate at which your battery can discharge, in kilowatts.

**How it's used:** Limits how much battery power ODIN can assign to any single hour. Setting this to your battery's actual maximum prevents the solver from over-allocating.

**If you have no battery:** leave at the default value — it has no effect when SoC is 0.

**Examples:**

| Battery system | Max discharge |
|----------------|--------------|
| Typical home battery (e.g. 5 kWh) | 2.5–3.0 kW |
| Larger system (10+ kWh) | 5.0–7.5 kW |

---

### 11.4 Heating System Type

This selector (in the Asgard Settings tab under Auto Adaptive) tells the solver what kind of heating system you have. It sets the default COP and thermal mass ranges.

| Option | Description |
|--------|-------------|
| **UFH** (Underfloor Heating) | Slow-response system with large thermal mass. Best for pre-heating strategies. |
| **Radiators** | Fast-response system with low thermal mass. Less benefit from pre-heating. |
| **UFH + Radiators** | Combination of Slow-response UFH and fast responding radiators |

---

## 12. Monitoring Performance — Room Temperature Charts

The best way to evaluate how well ODIN is working is to watch the **Room Temperatures (24h)** chart in the Solver tab. This chart shows three lines:

| Line | Description |
|------|-------------|
| **Scheduled** (blue dashed) | Your comfort target temperature (`sched_base`) for each hour |
| **Expected** (orange dashed) | What ODIN predicted the temperature would be when it made the plan |
| **Actual** (yellow solid) | What the room temperature actually measured throughout the day |

### What good performance looks like

- **Actual closely follows Expected** — the physics model is accurate. The house behaves as predicted.
- **Actual stays within the comfort band** — the solver is meeting its comfort constraints.
- **Expected rises during sunny hours without HP running** — passive solar gain is being correctly modelled.
- **Expected dips slightly during expensive hours, then recovers** — the solver is shifting heating to cheap periods.

### What to look for when tuning

**Actual consistently higher than Expected:**
The house is better insulated than the model thinks — Tau may be slightly underestimated. This is safe behaviour (you stay warm) and will self-correct as the learning converges. No action needed.

**Actual consistently lower than Expected:**
The house loses heat faster than modelled — heat loss may be underestimated, or there is an unmeasured draught. Consider increasing the `kWh Penalty` to make the solver plan more conservatively.

**Expected is flat during sunny hours (no solar warmup):**
The `Passive Solar Factor` may be too low. Increase it slightly and monitor over a few days.

**Expected shows large swings between hours:**
The comfort band may be too wide, or `kWh Penalty` is too low. Try increasing the penalty to 0.5–0.7 for more stable planning.

**Actual drops below sched_min:**
The solver underestimated heat loss or overestimated solar gain. Check that the Tau value is realistic for your house type. You can temporarily increase the `sched_min` offset to give more headroom.

### Checking yesterday vs today

The charts show 48 hours of data — yesterday (left half) and today (right half). This lets you compare what was planned versus what actually happened, and spot any systematic deviations between expected and actual temperature over a full day.