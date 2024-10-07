# M5Stack Atom Lite Proxy Example

This will need some soldering and some pins/sockets to connect to the Atom but works fine without any Grove or ST PAP-05V-S connectors.

```
  HP CN105                                                               WiFi If  
┌─────────┐                                                            ┌─────────┐
│  1 12V  ┼────────────────────────────────────────────────────────────┼  1 12V  │
│         │                                                            │         │
│  2 Gnd  ┼────────────┬───────────────────────────────────────────────┼  2 Gnd  │
│         │            │                                               │         │
│  3 5V   ┼────────────┼─┬─────────────────────────────────────────────┼  3 5V   │
│         │            │ │                                             │         │
│  4 Tx   ┼──────────┐ │ │                                    ┌────────┼  4 Tx   │
│         │          │ │ │                                    │        │         │
│  5 Rx   ┼────────┐ │ │ │                                    │   ┌────┼  5 Rx   │
│         │        │ │ │ │                                    │   │    │         │
└─────────┘        │ │ │ │                                    │   │    └─────────┘
                   │ │ │ │                                    │   │               
                   │ │ │ │          Atom Lite                 │   │               
                   │ │ │ │  ┌────────────────────────┐        │   │               
                   │ │ │ │  │                        │        │   │               
                   └─┼─┼─┼──┼  GPIO21                │        │   │               
                     │ │ │  │                        │        │   │               
                     └─┼─┼──┼  GPIO25                │        │   │               
                       │ │  │                        │        │   │               
                       │ └──┼  5V            GPIO23  ┼────────┘   │               
                       │    │                        │            │               
                       └────┼  Gnd           GPIO33  ┼────────────┘               
                            │                        │                            
                            └────────────────────────┘                            
```


You will also need to include the device config and add the second UART in your `ecodan-esphome.yaml` device yaml
```
packages:
  base: !include esphome-ecodan-hp/confs/base.yaml
  esp32: !include esphome-ecodan-hp/confs/m5stack-atom-lite-proxy.yaml
  ...
```

```
...
ecodan:
  id: ecodan_instance
  uart_id: uart_main
  proxy_uart_id: uart_proxy
```
