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

Including `confs/status_led_rgb.yaml` will show different colors on the Atom's RGB LED depending on state of esp module, wifi or api connection, and heatpump status:

```
# all ok - white
# app error - pulse red
# app warning - pulse yellow
# ap enabled - pulse white
# on network disconnected - fast pulse red
# api disconnected - fast pulse yellow
# hp error - 
# heating - orange
# cooling - blue
# dhw - purple