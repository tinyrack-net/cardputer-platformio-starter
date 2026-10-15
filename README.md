# M5Cardputer-UserDemo (PlatformIO)

하드웨어 확인용 M5Cardputer 사용자 데모. 업스트림
[M5Stack/M5Cardputer-UserDemo](https://github.com/m5stack/M5Cardputer-UserDemo) ESP-IDF 프로젝트를
PlatformIO 프로젝트로 이전했다. Arduino가 아니며, Windows 네이티브를 기준으로 쓰여 있다(리눅스도 동일하게
동작한다).

> **Cardputer ADV를 사용한다면 [CardputerADV 브랜치](https://github.com/m5stack/M5Cardputer-UserDemo/tree/CardputerADV)를 사용한다.**

#### 툴체인

[PlatformIO Core](https://docs.platformio.org/) (`espressif32@6.6.0`) + [ESP-IDF v4.4.6](https://docs.espressif.com/projects/esp-idf/en/v4.4.6/esp32s3/index.html)

ESP-IDF 툴체인과 관리 컴포넌트는 첫 빌드 때 PlatformIO가 자동으로 내려받는다(네트워크 필요, 수 분).

#### 빌드

```bash
pio run
```

#### 플래시 · 모니터

```bash
pio run -t upload     # 플래시 후 앱을 부팅시키고 부팅 로그를 출력
pio device monitor    # 시리얼 모니터 (대화형 터미널 필요)
pio run -t appboot    # 플래시 없이 다운로드 모드 탈출 / 앱 재실행
```

PlatformIO가 USB ID(`303a:1001`)로 보드를 찾으므로 포트를 지정할 필요가 없다 — Windows는 `COM3`,
리눅스는 `/dev/ttyACM0`. 다른 어댑터가 먼저 잡히면 `--upload-port COM3` / `--port COM3`처럼 명시한다.

Cardputer에서 업로드는 끝이 아니다. ESP32-S3는 내장 USB-Serial/JTAG 주변장치로 호스트와 통신하는데, 이
장치가 클래식 USB-UART 브리지의 EN/GPIO0 자동 리셋 회로를 DTR/RTS 라인으로 흉내 낸다. esptool의
"RTS 핀 하드 리셋"은 DTR을 asserted 상태로 남기고, 흉내 낸 회로는 그것을 "GPIO0 low"로 읽는다. 그래서
칩이 곧바로 다운로드 모드로 다시 리셋되고, 방금 플래시한 보드는 화면이 까만 채로 남을 수 있다(이때는
전원을 한 번 재인가해야 한다).

`scripts/pio_app_boot.py`가 매 업로드 후 실행되어 그 다운로드 모드를 끝낸다. DTR/RTS를 de-assert한
상태로 포트를 열고(실제 브리지가 하는 자동 리셋) 애플리케이션과 USB 장치를 살린 뒤, 캡처한 부팅 로그를
출력한다. 콘솔이 조용하면 ROM 부트로더를 통해 RTC 워치독을 무장시켜 *전체* 시스템 리셋으로 넘어간다
(esptool 4.7 이상의 `--after watchdog-reset`을, PlatformIO가 함께 제공하는 esptool 4.5.1용으로 재구현).
`pio run -t appboot`은 플래시 없이 같은 동작만 한다. 이 훅은 호스트 종류와 무관하다: 보드 포트를 pyserial과
USB ID로 찾으므로 Windows(`COMx`)와 리눅스(`/dev/ttyACM*`)에서 똑같이 동작한다.

#### 디버그

```bash
pio debug            # 내장 USB-JTAG로 GDB + OpenOCD (VSCode: "PIO Debug")
```

Cardputer는 ESP32-S3 JTAG을 같은 내장 USB-Serial/JTAG 포트로 노출하므로 별도 프로브가 필요 없다. GDB는
플랫폼이 제공하는 Xtensa 빌드(`tool-xtensa-esp-elf-gdb`)이고, OpenOCD는 기본 설치되지 않는다:

```bash
pio pkg install -g -t "platformio/tool-openocd-esp32@~2.1100.0"
```

OpenOCD는 `303a:1001`에 대한 raw USB 접근이 필요한데, 기본 상태에서는 허용되지 않는다.

Windows에서는 (복합 USB-Serial/JTAG 장치의 `MI_02` 인터페이스인) JTAG 인터페이스에 범용 WinUSB
드라이버가 물려 있어 OpenOCD가 `libusb_open() failed with LIBUSB_ERROR_NOT_FOUND`로 실패한다.
[ESP-IDF 문서](https://docs.espressif.com/projects/esp-idf/en/v4.4.6/esp32s3/api-guides/jtag-debugging/configure-builtin-jtag.html)가
이 오류의 해결책으로 제시하는 것은 Espressif의 WinUSB 드라이버다. 관리자 셸에서 한 번 설치하고 보드를
재연결한다. ESP-IDF Tools Installer에서는 "Espressif - WinUSB support for JTAG (ESP32-C3/S3)" 항목이
같은 역할을 한다:

```powershell
Invoke-WebRequest 'https://dl.espressif.com/dl/idf-env/idf-env.exe' -OutFile .\idf-env.exe
.\idf-env.exe driver install --espressif
```

리눅스에서는 장치 노드가 root 전용이라 `libusb_open() failed with LIBUSB_ERROR_ACCESS`로 실패한다.
`303a:1001` udev 규칙을 한 번 추가한다:

```bash
echo 'SUBSYSTEMS=="usb", ATTRS{idVendor}=="303a", ATTRS{idProduct}=="1001", MODE="0666", GROUP="plugdev"' \
    | sudo tee /etc/udev/rules.d/60-espressif-usb-jtag.rules
sudo udevadm control --reload-rules && sudo udevadm trigger --subsystem-match=usb
```

JTAG 없이도 패닉은 시리얼 모니터로 읽을 수 있다. `platformio.ini`의 `esp32_exception_decoder` 필터가
백트레이스를 파일/라인 정보로 바꿔 준다.

#### 구조

- `main/` — ESP-IDF 프로젝트 컴포넌트 (업스트림 레이아웃 유지, `platformio.ini`의 `src_dir` 참고).
- `components/` — 프로젝트 컴포넌트. `infrared_tools`만 벤더링해 두었고 나머지는 관리 컴포넌트로 해결한다.
- `main/idf_component.yml` — 의존성 매니페스트. `m5stack/m5gfx`(0.1.16), `espressif/esp-now`(2.5.1),
  `leeebo/tinyusb_src`(0.16.0~4)는 ESP Component Registry에서, `mooncake`(tag `v1.0.2`)는 Git에서 받는다.
  내려받은 것은 `managed_components/`에 들어가며 추적하지 않는다.
- `sdkconfig.defaults` — 보드 설정(8 MB flash, 사용자 `partitions.csv`, BLE, USB HID device).
  생성물 `sdkconfig.m5stack-cardputer`는 추적하지 않는다.

#### 환경 관련 주의

- `platformio.ini`는 `espressif32@6.6.0`, `framework-espidf@~3.40406.0`(v4.4.6),
  `toolchain-xtensa-esp32s3@8.4.0+2021r2-patch5` GCC 툴체인을 고정한다. 최신 PlatformIO 릴리스는 GCC 13
  툴체인을 선택하는데, 그것으로는 ESP-IDF v4.4를 컴파일하지 못한다(format-string, `sre_constants` 관련
  문제).
- PlatformIO는 ESP-IDF의 Python 의존성을 PlatformIO Core 옆인 `~/.platformio/penv/.espidf-4.4.6`
  (Windows는 `%USERPROFILE%\.platformio\penv\.espidf-4.4.6`)에 둔다. PlatformIO는 ESP-IDF < 5를 위해
  `pyparsing<2.4`를 고정하는데, 그 버전은 Python 3.13+에서 import할 수 없다(`sre_constants` 제거됨).
  그런 호스트에서는 Python 3.8~3.12로 가상환경을 다시 만들고 PlatformIO 의존성을 설치해야 한다:

  ```powershell
  $VENV = "$env:USERPROFILE\.platformio\penv\.espidf-4.4.6"
  py -3.12 -m venv $VENV                       # Python 3.8 - 3.12, 3.13+는 안 된다
  & "$VENV\Scripts\python.exe" -m pip install -U "urllib3<2" "cryptography>=2.1.4,<35.0.0" `
      "future>=0.18.3" "pyparsing>=2.0.3,<2.4.0" "kconfiglib~=13.7.1" "idf-component-manager~=1.0" `
      "esp-idf-kconfig>=1.4.2,<2.0.0"
  '{"version": "1.0.0"}' | Set-Content "$VENV\pio-idf-venv.json"
  ```
- Windows에서는 플래시·모니터에 드라이버 설치나 권한 설정이 필요 없다: 보드가 `USB Serial Device (COMx)`로
  나타난다. 리눅스에서는 `/dev/ttyACM0` 접근 권한을 대신 부여한다(`dialout` 그룹, 또는 보드 연결 후
  `sudo chmod 666 /dev/ttyACM0`).
- 애플리케이션을 손으로 시작할 일은 없다: `pio run -t upload`가 플래시한 펌웨어가 실제로 실행되도록
  끝맺는다("플래시 · 모니터" 참고). 그래도 보드가 다운로드 모드에 남으면(업로드 실패 후, 또는 수동으로
  다운로드 모드에 들어간 뒤) `pio run -t appboot`으로 빠져나오고, 그다음은 전원을 재인가한다.
- `pio device monitor`는 비대화형 셸에서 시작할 수 없다(stdin에 TTY가 필요하다). 파이프나 리다이렉트로는
  실행할 수 없고 터미널 창에서 실행한다.
- C/C++ 확장은 Xtensa/ESP-IDF 타깃을 표현할 수 없다. 호스트 IntelliSense 모드만 알고 있어, 분류할 수 없는
  컴파일러에 대해서는 타깃 매크로 위에 호스트 매크로를 덧붙인다 — `linux-gcc-x64` 모드는
  `__linux__`/`__unix__`/`__i386__`를, `windows-msvc-x64` 모드는 `_WIN32`/`_MSC_VER`를 추가한다. 그러면
  헤더 온리 spdlog(`<mooncake.h>`로 포함)가 리눅스 분기(`#include <sys/syscall.h>`)나 Windows
  분기(`#include <windows.h>`)를 타는데, 둘 다 ESP-IDF의 newlib에 없다 — `pio run`은 정상 컴파일되는데
  모든 앱 소스에 `cannot open source file` 물결 표시가 뜨는 이유다. `.vscode/settings.json`
  (`C_Cpp.default.forcedInclude`)과 `.vscode/intellisense/esp_idf_undef_host_macros.h`가 그 호스트
  매크로들을 IntelliSense에서만 undef해 에디터를 실제 빌드와 일치시킨다(vscode-cpptools#4653). 빌드
  과정에서 이 헤더를 읽는 곳은 없다.

#### 업스트림 소스와의 차이

- `main/hal/hal.h`가 벤더링된 `"../../components/M5GFX/src/M5GFX.h"` 대신 `<M5GFX.h>`를 포함한다.
  M5GFX가 이제 관리 컴포넌트로 해결되기 때문이다.
