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
pio run -t upload     # flash
pio device monitor    # serial monitor
```

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

#### Differences from the upstream source

- `main/hal/hal.h` includes `<M5GFX.h>` instead of the vendored
  `"../../components/M5GFX/src/M5GFX.h"`, because M5GFX is resolved as a managed component now.
