# ESP-01S Serial Remote Controller

This repository contains custom firmware for the ESP-01S that runs a web server,
which hosts a webpage. This webpage allows users to toggle values on a separate
microcontroller that connects via UART.

<div style="text-align: center;">
    <img src="img/esp01s.jpg" style="padding: 10px;" width="35%">
    <img src="img/esp01s-pinout.jpg" style="padding: 10px;" width="50%">
    <p align="center"><em>ESP-01S Module </em></p>
</div>

## Flashing

The firmware has been developed using PlatformIO and the Arduino framework. To
flash the firmware, connect the ESP module to a computer using a UART to USB
adapter. The ESP must be in programming mode, this is achived by holding
GPIO0 low during MCU initilization. Once the ESP is ready, flash the firmware
using PlatformIO console tools.

First flash the main firmware.

```console
$ pio run -t upload
```

Finally upload the files to the LittleFS filesystem.

```console
$ pio run -t uploadfs
```

## LICENSE

This software is licensed under the MIT License. See the [LICENSE](./LICENSE)
file for details.
