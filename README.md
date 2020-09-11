AIS Receiver Firmware
---------------------

This repository contains firmware (WIP) for my [AIS receiver](https://github.com/eugmes/ais-recv).

The firmware is writen in C and uses [Zephyr](https://zephyrproject.org).
[west tool](https://docs.zephyrproject.org/latest/guides/west/) is required
to build the application:

```console
% git clone https://github.com/eugmes/ais-recv-fw.git
% cd ais-recv-fw.git
% west init application
% west build
% west flash
```
