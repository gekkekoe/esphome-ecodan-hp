# ESPHome Ecodan Heat Pump — Documentation

Local control and monitoring for Mitsubishi Ecodan / Zubadan air-to-water heat
pumps, built on ESPHome and the CN105 service-port protocol. Runs standalone or
with Home Assistant, with no cloud dependency.

New here? Start with **Getting started**, pick your hardware, flash the firmware,
then set up Auto Adaptive Control.

---

## Getting started

| Guide | What it covers |
|-------|----------------|
| [Recommended hardware](hardware.md) | Which board to buy or build, and how to wire it to the CN105 port. |
| [Proxy setup](proxy.md) | Keep a MelCloud/Procon adapter working alongside the ESP. |
| [Install from prebuilt binaries](install-from-bin.md) | The fast path: flash a released `.factory.bin` and configure Wi-Fi. |
| [Build from source](build-from-source.md) | Compile your own firmware with ESPHome (board, zones, language, features). |

## Control & optimisation

| Guide | What it covers |
|-------|----------------|
| [Auto Adaptive Control](auto-adaptive.md) | The self-steering flow-temperature algorithm: profiles, setpoint bias, smart boost, and how to set it up. |
| [Auto Adaptive simulator](auto-adaptive-sim.html) | Interactive tool to see how the algorithm calculates flow temperature. |
| [Short-cycle mitigation](short-cycle-mitigation.md) | Protects the compressor from excessive on/off cycling. |

## Help

| Guide | What it covers |
|-------|----------------|
| [FAQ](faq.md) | Wi-Fi drops, short cycling, missing sensors in proxy mode, log viewing, DHW. |
| [Log viewer](logviewer.html) | Browser-based live log stream you can save to a file. |

## Asgard hardware

The [Asgard PCB](https://github.com/gekkekoe/esphome-ecodan-hp/blob/main/asgard/README.md)
is the recommended plug-and-play board. Its guides live alongside the hardware:

- [Hardware manual](https://github.com/gekkekoe/esphome-ecodan-hp/blob/main/asgard/manual.md) — physical install, wiring, first boot.
- [Home Assistant setup](https://github.com/gekkekoe/esphome-ecodan-hp/blob/main/asgard/ha-config.md) — Auto Adaptive from Home Assistant.
- [Standalone setup](https://github.com/gekkekoe/esphome-ecodan-hp/blob/main/asgard/sa-config.md) — the local dashboard, no Home Assistant needed.


---

*This project is independent and not affiliated with, endorsed by, or associated
with Mitsubishi Electric. Trade names are used for identification only.*
