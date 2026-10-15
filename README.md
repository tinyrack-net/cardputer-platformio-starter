# M5Cardputer-UserDemo (PlatformIO)

M5Cardputer user demo for hardware evaluation, migrated from the upstream
[M5Stack/M5Cardputer-UserDemo](https://github.com/m5stack/M5Cardputer-UserDemo)
ESP-IDF project to a PlatformIO project.

> **Note: If you are using Cardputer ADV, please use the [CardputerADV branch](https://github.com/m5stack/M5Cardputer-UserDemo/tree/CardputerADV).**

#### Tool Chains

[PlatformIO Core](https://docs.platformio.org/) (`espressif32@6.6.0`) + [ESP-IDF v4.4.6](https://docs.espressif.com/projects/esp-idf/en/v4.4.6/esp32s3/index.html)

The ESP-IDF toolchain and the managed components are downloaded automatically by
PlatformIO on the first build.

#### Build

```bash
pio run
```

#### Flash and monitor

```bash
# WSL: attach the board first with `usbipd wsl attach -a -b [BUSID]`
./flash.sh            # pio run -t upload + serial monitor on /dev/ttyACM0
```

```bash
pio run -t upload     # flash (and make sure the new firmware actually starts)
pio device monitor    # serial monitor
pio run -t appboot    # leave download mode / restart the application on demand
```

Uploading is not the end of the story on the Cardputer: the ESP32-S3 talks to the host through its
built-in USB-Serial/JTAG peripheral, which emulates the EN/GPIO0 auto-reset circuitry of a classic
USB-to-UART bridge on the DTR/RTS lines. esptool's "hard reset via RTS pin" leaves DTR asserted, which
that emulation reads as "GPIO0 low", so the chip resets straight back into download mode and a freshly
flashed board can sit there with a blank display (it then needs a power cycle).

`scripts/pio_app_boot.py` runs after every upload and ends that download mode: it opens the port with
DTR/RTS de-asserted - the auto-reset a real bridge would perform, which boots the application and keeps
the USB device alive - and prints the captured boot log. If the console stays silent it falls back to a
*full* system reset by arming the RTC watchdog through the ROM bootloader (what esptool >= 4.7 offers as
`--after watchdog-reset`, reimplemented here for the esptool 4.5.1 that PlatformIO ships), and
re-attaches the board with `usbipd` when WSL loses it on re-enumeration. `pio run -t appboot` does the
same thing without flashing.

#### Debug

```bash
pio debug            # GDB + OpenOCD over the on-board USB-JTAG (VSCode: "PIO Debug")
```

The Cardputer exposes its ESP32-S3 JTAG through the same built-in USB-Serial/JTAG port, so no extra
probe is needed. GDB is a Xtensa build from the platform (`tool-xtensa-esp-elf-gdb`); OpenOCD is not
installed by default:

```bash
pio pkg install -g -t "platformio/tool-openocd-esp32@~2.1100.0"
```

OpenOCD needs raw USB access to `303a:1001`, which is not granted by default (it fails with
`libusb_open() failed with LIBUSB_ERROR_ACCESS`). One-time fix, run with `sudo` and re-attach the board
(this is included in `./hotplugSetup.sh`):

```bash
echo 'SUBSYSTEMS=="usb", ATTRS{idVendor}=="303a", ATTRS{idProduct}=="1001", MODE="0666", GROUP="plugdev"' \
    | sudo tee /etc/udev/rules.d/60-espressif-usb-jtag.rules
sudo udevadm control --reload-rules && sudo udevadm trigger --subsystem-match=usb
```

Without JTAG, panics are still readable from the serial monitor: the `esp32_exception_decoder` filter
in `platformio.ini` turns the backtrace into file/line information.

#### Layout

- `main/` — the ESP-IDF project component (kept in the upstream layout, see `src_dir` in `platformio.ini`).
- `components/` — project components. Only `infrared_tools` is vendored here; everything else is resolved as a managed component.
- `main/idf_component.yml` — dependency manifest. `m5stack/m5gfx` (0.1.16), `espressif/esp-now` (2.5.1) and
  `leeebo/tinyusb_src` (0.16.0~4) come from the ESP Component Registry, `mooncake` (tag `v1.0.2`) from Git.
  They are downloaded into `managed_components/` (not tracked).
- `sdkconfig.defaults` — board configuration (8 MB flash, custom `partitions.csv`, BLE, USB HID device).
  The generated `sdkconfig.m5stack-cardputer` is not tracked.

#### Environment notes

- `platformio.ini` pins `espressif32@6.6.0`, `framework-espidf@~3.40406.0` (v4.4.6) and the
  `toolchain-xtensa-esp32s3@8.4.0+2021r2-patch5` GCC toolchain. Newer PlatformIO releases select the
  GCC 13 toolchain, which does not compile ESP-IDF v4.4 (format-string and `sre_constants` era breakage).
- PlatformIO keeps ESP-IDF's Python dependencies in `~/.platformio/penv/.espidf-4.4.6`. PlatformIO pins
  `pyparsing<2.4` for ESP-IDF < 5, and that version cannot be imported on Python 3.13+ (`sre_constants`
  was removed). On such hosts the virtual env has to be created with a Python <= 3.12 and PlatformIO's
  dependencies installed into it, e.g.:

  ```bash
  PY=$(mise where python@3.12)/bin/python3.12   # or any Python 3.8 - 3.12
  VENV=$HOME/.platformio/penv/.espidf-4.4.6
  $PY -m venv "$VENV"
  $VENV/bin/python -m pip install -U "urllib3<2" "cryptography>=2.1.4,<35.0.0" "future>=0.18.3" \
      "pyparsing>=2.0.3,<2.4.0" "kconfiglib~=13.7.1" "idf-component-manager~=1.0" "esp-idf-kconfig>=1.4.2,<2.0.0"
  printf '{\n  "version": "1.0.0"\n}\n' > "$VENV/pio-idf-venv.json"
  ```
- Flashing needs write access to `/dev/ttyACM0` (WSL: `usbipd wsl attach ...`). Add your user to the
  `dialout` group (`sudo usermod -aG dialout $USER`, re-login) or run `sudo chmod 666 /dev/ttyACM0`
  after each attach.
- The application itself never has to be started by hand: `pio run -t upload` finishes by making
  sure the flashed firmware runs (see "Flash and monitor"). If the board ever stays in download mode
  anyway (e.g. after a failed upload, or after entering download mode manually), `pio run -t appboot`
  gets it out, otherwise power-cycle the board once.
- `pio device monitor` cannot be started from a non-interactive shell (it requires a TTY on stdin);
  use `./flash.sh` or run it from a terminal.
- The USB-JTAG device needs the udev rule from `./hotplugSetup.sh` before `pio debug` can talk to it
  (see "Debug"); serial monitor and flashing only need `/dev/ttyACM0` access.
- The C/C++ extension cannot describe an Xtensa/ESP-IDF target, so it maps this project to the host
  `linux-gcc-x64` IntelliSense mode and predefines `__linux__`/`__unix__`/`__i386__`. Header-only spdlog
  (included through `<mooncake.h>`) then takes its Linux branch and `#include <sys/syscall.h>`, a glibc
  header ESP-IDF's newlib does not ship - the `cannot open source file "sys/syscall.h"` squiggle in every
  app source, while `pio run` compiles fine. `.vscode/settings.json` (`C_Cpp.default.forcedInclude`) plus
  `.vscode/intellisense/esp_idf_undef_host_macros.h` undefine those host macros for IntelliSense only, so
  the editor matches the real build (vscode-cpptools#4653). Nothing in the build reads that header.

#### Differences from the upstream source

- `main/hal/hal.h` includes `<M5GFX.h>` instead of the vendored
  `"../../components/M5GFX/src/M5GFX.h"`, because M5GFX is resolved as a managed component now.
