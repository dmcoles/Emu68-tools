# Wireless driver for Emu68

## CM4 and external antenna

Since it is meant for industrial applications, CM4 module features a connector for external antenna which may allow you to get stronger WiFi signals. The CM4 has a GPIO controlable switch which is used to select between built-in and external antenna. The corresponding GPIO is hidden behind the VPU and is not available to ARM or M68k directly. The selection of antenna is done by adding a parameter to the ``config.txt``file. The option is effective only in case of CM4 and has no effect on other RaspberryPi models, hence it is safe to leave it there if you swap the Pi model.

### Internal antenna

Internal antenna is enabled by default and requires no further changes. However, if really needed, one can add a line ```dtparam=ant1``` to the ``config.txt`` file on Emu68's boot partition.

### External antenna

If an external antenna shall be used, one need to add a line ```dtparam=ant2``` to the ``config.txt`` file. The option will be recognized by the wifipi.device on start and applied.