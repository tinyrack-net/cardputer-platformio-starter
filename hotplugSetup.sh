#!/bin/bash
# Setup that allows a non-root user to reach the Cardputer once it is attached to
# WSL. Run it once with sudo, then attach the board:
#   < usbipd wsl attach -a -b [BUSID] >  (PowerShell)
# Details: https://github.com/dorssel/usbipd-win/wiki/WSL-support

# Serial port (/dev/ttyACM0): flashing, `pio run -t upload`, `pio device monitor`.
sudo service udev restart
sudo udevadm control --reload

# ESP32-S3 built-in USB-Serial/JTAG (303a:1001) as a raw USB device, which is
# what OpenOCD needs for `pio debug`. Without it OpenOCD fails with
# "libusb_open() failed with LIBUSB_ERROR_ACCESS".
RULES=/etc/udev/rules.d/60-espressif-usb-jtag.rules
if [ ! -f "$RULES" ]; then
    echo 'SUBSYSTEMS=="usb", ATTRS{idVendor}=="303a", ATTRS{idProduct}=="1001", MODE="0666", GROUP="plugdev"' \
        | sudo tee "$RULES" > /dev/null
    sudo udevadm control --reload-rules
fi
sudo udevadm trigger --subsystem-match=usb
