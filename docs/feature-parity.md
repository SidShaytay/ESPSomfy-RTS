# Where each feature lives: web UI and LCD

Every user-facing controller feature belongs in both the web UI and the LILYGO screen, unless this table gives a reason it doesn't. A change that adds or moves a feature updates this table in the same pull request.

Paths use the standard LCD layout (public v4.1.0 and later). In the web UI, the gear icon opens the configuration tabs.

| Feature | Web UI | LCD | Notes |
|---|---|---|---|
| Control devices and groups | Home | Home → Devices / Groups / Rooms, or Quick access | |
| Rooms, devices and groups (add, edit, delete, order) | Somfy → Rooms / Shades / Groups | System → Configure → Rooms, devices & groups | |
| Pairing guide (pair or unpair a motor) | Somfy → Shades → edit shade → Pair Shade / Unpair Shade | Device → Advanced → Pair/unpair guide | |
| Group members, linked remotes, repeaters | Somfy → Shades / Groups / Repeater | Device or group → Advanced; Configure → Rooms, devices & groups → More → Repeaters | |
| General, security, network, MQTT and radio settings | System → Options / Security; Network; Radio | System → Configure → General / Security / Network / MQTT / Radio | |
| Wi-Fi scan | Network → Adapter | System → Configure → Network → WiFi scan | |
| Battery level | Header, under Network and Channel; System → Options → Battery | Status screen; System → Configure → Battery | Shown only on boards with a battery gauge |
| Battery charge limit | System → Options → Battery | System → Configure → Battery | 60% recommended for a controller always on USB; see [battery charge limit](battery-charge-limit.md) |
| GitHub updates (check, install, cancel) | System → Firmware → GitHub releases | System → Maintenance → GitHub updates (Home → Updates) | |
| Restart | System → Options → Reboot | System → Maintenance → Restart | |
| Receive log and frequency scan | Radio → RF frames; Radio → Transceiver → Scan | System → Diagnostics | Copying the log stays in the browser (the screen has no clipboard) |
| Backup, restore, manual firmware upload, refresh cache | System → Firmware | System → Maintenance → Browser files & Web shows where to do it | Browser only: these need a file picker or browser cache |
| Display brightness, dim and sleep | — | System → Configure → Display | Screen only: it controls the screen itself |
| Quick access shortcuts | — | System → Configure → Quick entries | Screen only: shortcuts for the encoder |
