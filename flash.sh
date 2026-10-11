#!/bin/bash
### This is for lazzy shit like me to build -> flash - > monitor project
### Use < usbipd wsl attach -a -b [BUSID] > on PowerShell to auto attach the board


# Configs
PIO=${PIO:-$HOME/.platformio/penv/bin/pio}
SERIAL_PORT=${SERIAL_PORT:-/dev/ttyACM0}


# Help shit
help() {
    sed -rn 's/^### ?//;T;p' "$0"
}

if [[ "$1" == "-h" ]] || [[ "$1" == "--help" ]]; then
    help
    exit 1
fi

# Build and flash and monitor
"$PIO" run -t upload --upload-port "${SERIAL_PORT}"
"$PIO" device monitor --port "${SERIAL_PORT}" --baud 115200
