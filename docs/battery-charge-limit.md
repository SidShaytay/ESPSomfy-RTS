# Battery level and charge limit

On the LILYGO T-Embed CC1101 (and Plus), **System → Firmware** in the web UI shows the battery level from the board's BQ27220 gauge, and the LCD shows it too. Boards without a battery show nothing.

## Charge limit

**Recommendation: 60% for a controller that runs on USB all the time.**

A lithium-ion cell wears fastest when it sits full and warm. That is what happens to a controller that is always plugged in: the BQ25896 charger fills the cell to 4.208 V, stops, and tops it up again whenever it drops 100 mV, so the cell spends its whole life at 95–100%. A lower limit trades backup runtime for a much longer life, and the lower the better. 60% (3.84 V) is the lowest the charger can be set to.

| Setting | Charge voltage | Cycle life (BU-808) |
|---|---|---|
| 100% (charger default) | 4.208 V | 300–500 |
| 90% | 4.128 V | 400–700 |
| 80% | 4.064 V | 600–1,000 |
| 70% | 3.968 V | between 850–1,500 (4.00 V) and 1,200–2,000 (3.92 V) |
| **60% (recommended)** | **3.840 V** | **2,400–4,000** |

Charge levels and cycle counts come from Battery University's [BU-808](https://batteryuniversity.com/article/bu-808-how-to-prolong-lithium-based-batteries) table of charge voltage against cycle life and stored energy, rounded to the charger's 16 mV steps. They are typical for this kind of cell, not measured on this board. For a battery that mostly sits idle, BU-808's storage table points the same way: after a year at 25 °C, a cell kept at 40% keeps 96% of its capacity and one kept full keeps 80%.

Use 100% only if the controller often runs on battery and needs the full runtime. At 60% a power cut gives roughly 60% of the full-charge runtime.

The gauge learns the cell's capacity from full charges and deep discharges, which a limited cell never sees, so its percentage may drift. The voltage shown next to it stays accurate.

## How it works

100% is the default, and then the firmware writes nothing to the charger. Choosing a lower limit puts the charger under the firmware's control: it turns the charger's I²C watchdog off (otherwise the charger would reset the voltage after 40 seconds) and sets the charge voltage. Only those register bits change. The firmware checks them every 30 seconds and sets them again if the charger lost power and reset. Going back to 100% restores both defaults, and the charger returns to running on its own.

The limit is saved in its own setting on the controller. It is not part of the backup file, so set it again after restoring onto a new board. If you flash firmware that predates this setting while a limit is active, the charger keeps that limit until it loses all power (USB unplugged and battery disconnected).
