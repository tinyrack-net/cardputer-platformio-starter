# Cardputer HID OS (PlatformIO)

M5Stack Cardputer용 키보드·마우스 전용 펌웨어. 업스트림
[M5Stack/M5Cardputer-UserDemo](https://github.com/m5stack/M5Cardputer-UserDemo)를 PlatformIO + ESP-IDF
v4.4.6으로 이전한 저장소에서 런처/데모 앱을 걷어내고, **단일 HID OS**만 실행하도록 개조했다. Arduino가
아니며, Windows 네이티브를 기준으로 쓰여 있다(리눅스도 동일하게 동작한다).

- USB / BLE 3슬롯 중 **한 채널만** 활성화되고, 키보드와 마우스를 함께 출력한다.
- 부팅 시 저장된 채널을 그대로 사용하며, USB 연결을 감지해 자동 전환하지 않는다.
- 부팅 시 항상 키보드 모드이고, 마우스 모드는 런타임 토글이다.

#### 조작

| 입력 | 동작 |
|------|------|
| `fn+1` / `fn+2` / `fn+3` (짧게) | BT1 / BT2 / BT3 슬롯으로 전환 |
| `fn+0` (짧게) | USB로 전환 |
| `fn+1/2/3` (길게, 1초) | 해당 슬롯 본드 삭제 후 재페어링 대기 |
| `fn+M` | 마우스 모드 토글 (휘발성, 부팅 시 off) |
| `w/a/s/d` | 마우스 이동 (홀드 시 가속) |
| `r` / `f` | 휠 위 / 아래 |
| `space` / `enter` / `m` | 좌 / 우 / 중 클릭 |

BLE 기기명은 세 슬롯 모두 `M5-Keyboard`이며, Just Works + Bonding으로 페어링한다(핀 입력 없음).
HUD는 상단바에 채널·연결 상태·마우스 여부, 메인 캔버스에 슬롯 본드 상태, 우측 바에 배터리를 보여 준다.

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

> **USB 채널 주의.** 이 펌웨어는 USB 채널이 활성화되면 내장 USB PHY를 USB-OTG로 돌려 HID 장치가 된다.
> 그 순간 USB-Serial/JTAG 콘솔(플래싱·모니터에 쓰는 그 포트)은 사라진다. 이는 의도된 동작이며 부팅 시
> 시리얼을 붙잡아 두는 안전장치는 넣지 않았다. USB 채널에서 다시 플래시하려면 사용자가 BOOT를 누른 채
> 리셋해 수동으로 다운로드 모드에 들어간다. BLE 채널로 부팅하면 시리얼은 그대로 유지된다.

Cardputer에서 업로드는 끝이 아니다. ESP32-S3는 내장 USB-Serial/JTAG 주변장치로 호스트와 통신하는데, 이
장치가 클래식 USB-UART 브리지의 EN/GPIO0 자동 리셋 회로를 DTR/RTS 라인으로 흉내 낸다. esptool의
"RTS 핀 하드 리셋"은 DTR을 asserted 상태로 남기고, 흉내 낸 회로는 그것을 "GPIO0 low"로 읽는다. 그래서
칩이 곧바로 다운로드 모드로 다시 리셋되고, 방금 플래시한 보드는 화면이 까만 채로 남을 수 있다(이때는
전원을 한 번 재인가해야 한다).

`scripts/pio_app_boot.py`가 매 업로드 후 실행되어 그 다운로드 모드를 끝낸다. DTR/RTS를 de-assert한
상태로 포트를 열고(실제 브리지가 하는 자동 리셋) 애플리케이션과 USB 장치를 살린 뒤, 캡처한 부팅 로그를
출력한다. 콘솔이 조용하면 ROM 부트로더를 통해 RTC 워치독을 무장시켜 *전체* 시스템 리셋으로 넘어간다.
`pio run -t appboot`은 플래시 없이 같은 동작만 한다. 이 훅은 호스트 종류와 무관하다: 보드 포트를 pyserial과
USB ID로 찾으므로 Windows(`COMx`)와 리눅스(`/dev/ttyACM*`)에서 똑같이 동작한다.

#### 디버그

```bash
pio debug            # 내장 USB-JTAG로 GDB + OpenOCD (VSCode: "PIO Debug")
```

Cardputer는 ESP32-S3 JTAG을 같은 내장 USB-Serial/JTAG 포트로 노출하므로 별도 프로브가 필요 없다. OpenOCD는
기본 설치되지 않는다:

```bash
pio pkg install -g -t "platformio/tool-openocd-esp32@~2.1100.0"
```

Windows에서는 JTAG 인터페이스(`MI_02`)에 범용 WinUSB 드라이버를 물려야 OpenOCD가 동작한다. 관리자 셸에서
한 번 설치하고 보드를 재연결한다:

```powershell
Invoke-WebRequest 'https://dl.espressif.com/dl/idf-env/idf-env.exe' -OutFile .\idf-env.exe
.\idf-env.exe driver install --espressif
```

리눅스에서는 `303a:1001` udev 규칙을 한 번 추가한다:

```bash
echo 'SUBSYSTEMS=="usb", ATTRS{idVendor}=="303a", ATTRS{idProduct}=="1001", MODE="0666", GROUP="plugdev"' \
    | sudo tee /etc/udev/rules.d/60-espressif-usb-jtag.rules
sudo udevadm control --reload-rules && sudo udevadm trigger --subsystem-match=usb
```

JTAG 없이도 패닉은 시리얼 모니터로 읽을 수 있다. `platformio.ini`의 `esp32_exception_decoder` 필터가
백트레이스를 파일/라인 정보로 바꿔 준다.

#### 구조

```
main/
  cardputer.cpp                     HAL init → HID OS 단일 앱 실행 (런처 없음)
  apps/
    apps.h
    app_hid_os/
      app_hid_os.{h,cpp}            부팅/메인 루프/키 해석/HUD/이벤트 처리
      hid_hud.{h,cpp}               상태 화면 렌더링
    utils/                          HAL이 의존하는 유틸 (boot_sound, common_define, theme)
  hid/
    hid_manager.{h,cpp}             트랜스포트 추상화 + 채널 전환 + 리포트 라우팅
    usb_hid.{h,cpp}                 USB HID (TinyUSB, 키보드+마우스)
    ble_hid.{h,cpp}                 BLE HID (리포트맵, 3슬롯, Just Works 본딩, 화이트리스트 광고)
    config_store.{h,cpp}            NVS 영속화
    hid_keymap.{h,cpp}              물리키 → HID / 마우스 매핑
    tinyusb_hid.{h,c}               TinyUSB HID 리포트 helper
    usb_descriptors.{h,c}           USB 디스크립터 (키보드 + 마우스)
    tusb_config.h                   TinyUSB 설정
    esp_hid_gap.{h,c}               BLE controller/GAP 초기화 + 광고 + 보안
  hal/                              하드웨어 추상화 (그대로 유지)
components/
  infrared_tools/                   벤더링 유지 (현재 미사용)
main/idf_component.yml              의존성 매니페스트
sdkconfig.defaults                  보드 설정
partitions.csv                      파티션 레이아웃
```

`main/`은 ESP-IDF 프로젝트 컴포넌트이고(`platformio.ini`의 `src_dir`), `main/CMakeLists.txt`가
`apps/`, `hal/`, `hid/`를 글롭하므로 폴더를 추가해도 CMake 수정이 필요 없다.

#### NVS 데이터 모델

namespace `hid_os`:

| 키 | 타입 | 값 |
|----|------|----|
| `channel` | u8 | 0=USB, 1=BT1, 2=BT2, 3=BT3 |
| `slot0_addr` / `slot1_addr` / `slot2_addr` | blob(7) | `addr_type(1) + bd_addr(6)` |
| `mouse_speed` | u8 | 이동 감도 (기본 1) |
| `mouse_accel` | i8 | 가속 설정 (기본 on) |
| `device_name` | str | `M5-Keyboard` |

Bluedroid 본딩 키는 스택이 NVS에 자동 저장하며, 펌웨어는 슬롯↔주소 매핑만 관리한다. 부팅 시
`esp_ble_get_bond_device_list()`로 슬롯 유효성을 검증하고, 스택에 본드가 없는 슬롯은 정리한다.

#### 업스트림 소스와의 차이

- `main/hal/hal.h`가 벤더링된 `"../../components/M5GFX/src/M5GFX.h"` 대신 `<M5GFX.h>`를 포함한다
  (M5GFX가 관리 컴포넌트로 해결되기 때문).
- `main/hal/keyboard/keyboard.cpp`의 `isChanged()`가 눌린 키 개수 대신 디코드된 키 상태 해시를 비교한다.
- `main/hid/`의 TinyUSB helper와 `esp_hid_gap`은 업스트림 usb_keyboard_wrap/ble_keyboard_wrap에서
  옮겨 온 것이며, 모디파이어 처리·마우스 리포트·Just Works 보안·화이트리스트 광고를 위해 수정했다.

#### 환경 관련 주의

- `platformio.ini`는 `espressif32@6.6.0`, `framework-espidf@~3.40406.0`(v4.4.6),
  `toolchain-xtensa-esp32s3@8.4.0+2021r2-patch5` GCC 툴체인을 고정한다. 최신 플랫폼은 GCC 13 툴체인을
  선택하는데, 그것으로는 ESP-IDF v4.4를 컴파일하지 못한다.
- PlatformIO는 ESP-IDF의 Python 의존성을 `%USERPROFILE%\.platformio\penv\.espidf-4.4.6`에 둔다.
  Python 3.13+ 호스트에서는 그 venv를 Python 3.8~3.12로 다시 만들어야 한다(`pyparsing<2.4` 제약).
- 보드 설정은 `sdkconfig.defaults`로만 바꾸고, 생성물 `sdkconfig.m5stack-cardputer`는 커밋하지 않는다.
  수정 후에는 지우고 리빌드한다.
- `pio device monitor`는 비대화형 셸에서 시작할 수 없다(stdin에 TTY가 필요하다).
- C/C++ 확장은 Xtensa/ESP-IDF 타깃을 표현할 수 없다. `.vscode/settings.json`
  (`C_Cpp.default.forcedInclude`)과 `.vscode/intellisense/esp_idf_undef_host_macros.h`가 호스트 매크로를
  IntelliSense에서만 undef해 에디터를 실제 빌드와 일치시킨다.
