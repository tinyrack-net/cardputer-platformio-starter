"""
PlatformIO extra script: make sure the flashed application actually runs after
uploading to an ESP32-S3 over its built-in USB-Serial/JTAG port
(M5Stack Cardputer / M5Stamp S3).

Why this is needed
------------------
Flashing leaves the chip in download mode. On ESP32-S3 boards the host talks to the
chip through its USB-Serial/JTAG peripheral, which emulates the EN/GPIO0 auto-reset
circuitry of a classic USB-to-UART bridge on the DTR/RTS lines (esptool inverts them
on purpose, see its ``USBJTAGSerialReset``). esptool's plain "hard reset via RTS pin"
pulses RTS only and keeps DTR asserted, which the emulation reads as "GPIO0 low" ->
the chip resets straight back into download mode. After ``pio run -t upload`` the
board therefore looks dead (blank display, no application log) until it is power
cycled, even though the flash write itself succeeded.

Two ways out of download mode, both used by :func:`boot_application`:

* Open the port with DTR/RTS de-asserted, i.e. the same auto-reset sequence with
  "GPIO0 high" as a real bridge would produce. The chip re-samples the strapping
  pins, boots the application, and the USB device stays alive, so the boot log can
  be captured right away.
* Arm the RTC watchdog through the ROM bootloader, which forces a *full* system
  reset. This is what esptool >= 4.7 does for ``--after watchdog-reset``; the
  esptool bundled with platform-espressif32 6.6.0 (4.5.1) cannot do it, so the
  registers are written here directly. It is the fallback for a chip that stays in
  download mode anyway.

Either way the script prints the application log, so the upload result can be judged
from the console instead of the bare "write successful" of esptool.

References
----------
* https://docs.espressif.com/projects/esptool/en/latest/esp32s3/troubleshooting.html
  ("Leaving Download Mode in USB-Serial/JTAG Mode")
* esptool ``targets/esp32s3.py`` - ``ESP32S3ROM.watchdog_reset()``
"""

import contextlib
import io
import os
import sys
import time

try:  # pyserial, shipped with PlatformIO Core, on every host
    import serial
    import serial.tools.list_ports
except ImportError:  # pragma: no cover - only in a broken PlatformIO install
    serial = None

Import("env")  # noqa: F821 - provided by PlatformIO's SCons environment

# --- ESP32-S3 registers (soc/esp32s3/include/soc/rtc_cntl_reg.h) ---
RTC_CNTL_BASE = 0x60008000
RTC_CNTL_WDTCONFIG0_REG = RTC_CNTL_BASE + 0x98
RTC_CNTL_WDTCONFIG1_REG = RTC_CNTL_BASE + 0x9C
RTC_CNTL_WDTWPROTECT_REG = RTC_CNTL_BASE + 0xB0
RTC_CNTL_WDT_WKEY = 0x50D83AA1
RTC_CNTL_OPTION1_REG = 0x6000812C
RTC_CNTL_FORCE_DOWNLOAD_BOOT_MASK = 0x1
# enable WDT | stage0 = system reset | 2 us timeout scale
RTC_CNTL_WDTCONFIG0_VALUE = (1 << 31) | (5 << 28) | (1 << 8) | 2

# USB IDs of the ESP32-S3 built-in USB-Serial/JTAG: the CDC interface of that
# composite device is the port used here (``COMx`` on Windows, ``/dev/ttyACM*``
# on Linux).
USB_SERIAL_JTAG_VID = 0x303A
USB_SERIAL_JTAG_PID = 0x1001


def _log(message):
    print("[app-boot] %s" % message)
    sys.stdout.flush()


def _candidate_ports():
    """Every serial port that could be the board, most likely first.

    Sorting by USB ID keeps unrelated adapters at the back of the list, so a host
    that also has a legacy ``COM1`` still finds the board first.
    """

    def rank(port):
        if (port.vid, port.pid) == (USB_SERIAL_JTAG_VID, USB_SERIAL_JTAG_PID):
            return 0
        if port.vid == USB_SERIAL_JTAG_VID:
            return 1
        if port.vid is not None:
            return 2
        return 3  # a legacy UART, e.g. COM1

    ports = serial.tools.list_ports.comports()
    return [port.device for port in sorted(ports, key=lambda p: (rank(p), p.device))]


def _port_exists(port):
    """Is this still a usable serial port? (``COM3`` is not a filesystem path.)"""
    if os.path.exists(port):
        return True
    return port in _candidate_ports()


def _upload_port(env):
    port = (env.subst("$UPLOAD_PORT") or "").strip().strip('"')
    if port and _port_exists(port):
        return port
    ports = _candidate_ports()
    return ports[0] if ports else None


def _open_port(port):
    """Open the port without touching the control lines.

    Toggling RTS on a USB-Serial/JTAG chip issues a core reset (which would push the
    chip back into download mode), so both lines stay de-asserted. The write timeout
    matters as well: while the application runs, nothing drains its console input, so
    an unbounded write would block forever.
    """
    handle = serial.Serial()
    handle.port = port
    handle.baudrate = 115200
    handle.timeout = 0.2
    handle.write_timeout = 1.0
    handle.dtr = False
    handle.rts = False
    handle.open()
    return handle


def _close_port(handle):
    """Close the port without waiting for unread console input to drain.

    While the application runs nothing consumes its console input, so the USB CDC
    output buffer never empties and a plain close() can block for ~30 s. Discarding
    the untransmitted bytes first makes the close immediate.
    """
    try:
        if os.name == "posix":
            import termios  # flush the output queue only

            termios.tcflush(handle.fileno(), termios.TCOFLUSH)
        else:
            handle.reset_output_buffer()  # PurgeComm(PURGE_TXABORT | PURGE_TXCLEAR)
    except Exception:
        pass
    try:
        handle.close()
    except Exception:
        pass


def _esptool_s3_rom(env):
    package_dir = env.PioPlatform().get_package_dir("tool-esptoolpy")
    if package_dir and package_dir not in sys.path:
        sys.path.insert(0, package_dir)
    from esptool.targets.esp32s3 import ESP32S3ROM

    return ESP32S3ROM



def _connect_rom(rom_class, port, attempts=2):
    """Return (handle, ESPLoader) when the ROM bootloader answers, else (None, None)."""
    try:
        handle = _open_port(port)
    except Exception as error:
        _log("cannot open %s: %r" % (port, error))
        return None, None
    try:
        esp = rom_class(handle, baud=115200, trace_enabled=False)
        with contextlib.redirect_stdout(io.StringIO()):  # esptool is chatty
            esp.connect("no_reset", attempts=attempts)
    except Exception:
        _close_port(handle)
        return None, None
    return handle, esp


def _watchdog_reset(esp):
    """Full system reset via the RTC watchdog: leaves download mode, boots the app."""
    try:
        esp.update_reg(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT_MASK, 0)
    except Exception:
        pass  # the chip may already have reset
    esp.write_reg(RTC_CNTL_WDTWPROTECT_REG, RTC_CNTL_WDT_WKEY)  # unlock
    esp.write_reg(RTC_CNTL_WDTCONFIG1_REG, 2000)  # timeout
    try:
        esp.write_reg(RTC_CNTL_WDTCONFIG0_REG, RTC_CNTL_WDTCONFIG0_VALUE)  # fire
    except Exception:
        pass  # the chip resets before it can answer


def _wait_for_port(timeout=20.0):
    """Wait until the board re-enumerates as a usable serial port."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        for port in _candidate_ports():
            try:
                _close_port(_open_port(port))
                return port
            except Exception:
                continue
        time.sleep(0.5)
    return None


ROM_DOWNLOAD_MARKERS = ("waiting for download", "boot:0x3", "DOWNLOAD(USB/UART0)")
APP_MARKERS = (
    "[info]",
    "mooncake",
    "Launcher",
    "2nd stage bootloader",
    "Loaded app from partition",
)


def _guess_state(text):
    """Guess the board state from the console output that was pending on the port."""
    lowered = text.lower()
    download_at = -1
    for marker in ROM_DOWNLOAD_MARKERS:
        download_at = max(download_at, lowered.rfind(marker.lower()))
    app_at = -1
    for marker in APP_MARKERS:
        app_at = max(app_at, lowered.rfind(marker.lower()))
    if download_at < 0 and app_at < 0:
        return None
    return "download" if download_at > app_at else "app"


def _read_console(port, seconds=6.0, idle=1.0):
    """Read (never write) whatever the board has queued on its console."""
    try:
        handle = _open_port(port)
    except Exception as error:
        _log("cannot read %s: %r" % (port, error))
        return ""
    collected = []
    deadline = time.time() + seconds
    try:
        while time.time() < deadline:
            data = handle.read(4096)
            if data:
                collected.append(data.decode("utf-8", "replace"))
                deadline = time.time() + idle
    except Exception:
        pass
    _close_port(handle)
    return "".join(collected)


def _show_console(port, seconds=6.0):
    """Show the application console and say whether the app really is up."""
    log = _read_console(port, seconds=seconds)
    if log.strip():
        _log("console output on %s:" % port)
        sys.stdout.write(log if log.endswith("\n") else log + "\n")
        sys.stdout.flush()

    state = _guess_state(log)
    if state == "app":
        _log("application is running on %s" % port)
        return True
    if state == "download":
        _log(
            "board is still in download mode on %s and did not react to a full system "
            "reset; power-cycle the board once" % port
        )
        return False
    _log(
        "could not confirm the application on %s (no console output); check the display "
        "or `pio device monitor`, and power-cycle the board if it stays blank" % port
    )
    return False


def _reset_download_mode(port, esp, handle):
    """Full system reset out of download mode, then report the application log."""
    try:
        chip = esp.CHIP_NAME
        if "esp32-s3" not in chip.lower():
            _log("chip is %s, no full system reset needed" % chip)
            return True
        _log("board is in download mode on %s, triggering a full system reset" % port)
        _watchdog_reset(esp)
    except Exception as error:
        _log("full system reset failed: %r" % error)
        return False
    finally:
        _close_port(handle)

    time.sleep(1.0)
    port = _wait_for_port()
    if port is None:
        _log("board did not come back as a serial port; power-cycle it")
        return False
    return _show_console(port)


def boot_application(env):
    """Make sure the flashed application ends up running on the board.

    Two mechanisms, in the order that is kindest to the board:

    1. Opening the port de-asserts DTR/RTS, and the USB-Serial/JTAG peripheral
       treats that as the classic auto-reset ("external pin" reset): the chip is
       reset to run normally, so a chip that was left in download mode boots the
       application. The port stays usable, which is how the boot log below is
       captured. (This is what esptool's reset strategies rely on as well.)
    2. If the console stays silent the chip did not leave download mode. Then the
       ROM bootloader is asked to arm the RTC watchdog, which restarts the chip
       with a full system reset (esptool >= 4.7: ``--after watchdog-reset``).

    The application log decides whether the flash worked, so never trust the mere
    presence of bytes on the port: the console queue also replays the previous
    boot log after a reset.
    """
    if serial is None:
        _log("pyserial is not available, skipping")
        return

    port = _upload_port(env)
    if port is None:
        _log("no serial port found, skipping")
        return

    time.sleep(1.0)  # let the chip settle after the upload's reset

    if _show_console(port):
        return

    # Silent console: fall back to the full system reset of the ROM bootloader.
    try:
        rom_class = _esptool_s3_rom(env)
    except Exception as error:
        _log("esptool not available (%r), skipping the full system reset" % (error,))
        return

    handle, esp = _connect_rom(rom_class, port)
    if esp is None:
        _log("the ROM bootloader does not answer either; power-cycle the board")
        return
    if not _reset_download_mode(port, esp, handle):
        _show_console(port)


def _post_upload_action(target=None, source=None, env=None):
    """SCons passes its own environment as the ``env`` keyword argument."""
    boot_application(env)


env.AddPostAction("upload", _post_upload_action)
env.AddCustomTarget(
    name="appboot",
    dependencies=None,
    actions=[_post_upload_action],
    title="Boot app",
    description="Leave ESP32-S3 download mode and start the flashed application",
)
