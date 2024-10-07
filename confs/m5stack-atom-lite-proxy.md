# M5Stack Atom Lite Proxy Example

This will need some soldering and some pins to connect to the Atom.


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



You will also need to add the second UART at the end of your `ecodan-esphome.yaml` device config

```
ecodan:
  id: ecodan_instance
  uart_id: uart_main
  proxy_uart_id: uart_proxy
```