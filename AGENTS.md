# cardputer hid-os

M5Stack Cardputer용 M5Cardputer-UserDemo(mooncake 앱 프레임워크 + 자체 HAL)를 **PlatformIO + ESP-IDF
v4.4.6**으로 이전한 저장소를, **키보드·마우스 전용 단일 펌웨어(HID OS)** 로 개조한 프로젝트. Arduino가
아니다. 런처·데모 앱은 제거했고 앱은 하나뿐이다. 문서는 `README.md`와 이 파일만 유지한다.

## 빌드 · 실행

Windows 네이티브에서 작업한다(WSL 아님). PlatformIO Core는 PATH에 없고 사용자 프로필 venv에 있다.

```powershell
$PIO = "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe"

& $PIO run                # 빌드 (클린 빌드 ~2.5분)
& $PIO run -t upload      # 플래시 (앱 부팅까지 자동, 포트 자동 탐지)
& $PIO run -t appboot     # 플래시 없이 다운로드 모드 탈출/앱 재실행
& $PIO device monitor     # 터미널 필요, 파이프/리다이렉트 불가
```

- 보드는 내장 USB-Serial/JTAG로 연결하고 Windows에서는 `COMx`(VID:PID `303A:1001`)로 잡힌다. 시리얼
  플래시·모니터에 드라이버 설치나 udev/권한 설정이 필요 없다. 포트를 잘못 잡으면 `--upload-port COM3`처럼
  명시한다. `.pio`(빌드 산출물)는 호스트 간에 공유하지 않는다 — 호스트를 바꾸면 지우고 리빌드한다.
- esptool의 기본 리셋 때문에 업로드 후 칩이 다운로드 모드(화면 까맣)에 남는다. `scripts/pio_app_boot.py`가
  업로드 후 자동으로 앱을 부팅시키고 부팅 로그를 출력하므로, 업로드 성공 여부는 그 로그로 판단한다.
  그래도 화면이 까맣면 `pio run -t appboot`, 그다음은 전원 재인가. 훅은 pyserial 기반이라 Windows/Linux
  양쪽에서 동작한다.
- **USB 채널은 시리얼을 죽인다.** USB 채널이 활성화되면 내장 USB PHY가 USB-OTG로 돌아가 USB-Serial/JTAG
  콘솔이 사라진다. 부팅 채널이 USB면 플래시·모니터 포트가 없어지므로, 재플래시는 수동 다운로드 모드
  (BOOT 누른 채 리셋)로만 가능하다. 부팅 시 시리얼을 붙잡는 안전장치는 의도적으로 넣지 않았다(요구사항).
  BLE 채널로 부팅하면 시리얼이 유지된다. 개발 중에는 채널을 BLE로 두고 작업하면 편하다.
- JTAG 디버깅(`pio debug`)은 `platformio/tool-openocd-esp32` 설치가 필요하고, Windows에서는 Espressif의
  WinUSB 드라이버를 한 번 설치해야 한다(없으면 `LIBUSB_ERROR_NOT_FOUND`) — 명령은 `README.md` "Debug" 참고.
- 첫 빌드는 ESP-IDF 툴체인과 관리 컴포넌트를 내려받는다(네트워크 필요, 수 분). PlatformIO Core Python은
  3.11이라 ESP-IDF용 venv(`%USERPROFILE%\.platformio\penv\.espidf-4.4.6`)를 그대로 쓸 수 있다. Python
  3.13+ 호스트에서는 그 venv를 3.12 이하로 다시 만들어야 한다 — `README.md` "Environment notes" 참고.

## 지켜야 할 것

- `platformio.ini`의 버전 고정을 올리지 않는다: `espressif32@6.6.0` + `framework-espidf@~3.40406.0`(IDF 4.4.6)
  + `toolchain-xtensa-esp32s3@8.4.0`. 최신 플랫폼은 GCC 13을 붙여 ESP-IDF 4.4를 컴파일하지 못한다.
- 보드 설정(Kconfig)은 `sdkconfig.defaults`로만 바꾼다. 생성물 `sdkconfig.m5stack-cardputer`는 커밋하지
  않으며, 수정 후에는 지우고 리빌드한다.
- 의존성은 `main/idf_component.yml`에 선언하고 `managed_components/`는 커밋하지 않는다.
  `components/infrared_tools`만 벤더링 유지(레지스트리 미등록, 현재 미사용).
- `[platformio] src_dir = main` → `main/`이 ESP-IDF 메인 컴포넌트이고 `main/CMakeLists.txt`가 그대로 쓰인다
  (glob 방식이라 `apps/`·`hal/`·`hid/` 아래 파일을 추가해도 CMake 수정이 필요 없다).
- 루트 `CMakeLists.txt`의 `include_directories(./main/hid)`는 TinyUSB가 `tusb_config.h`를 찾게 하려는
  것이다. TinyUSB를 만지기 전에 이 경로를 유지한다.
- IntelliSense용 `.vscode/settings.json`(`C_Cpp.default.forcedInclude`)과
  `.vscode/intellisense/esp_idf_undef_host_macros.h`는 의도된 설정이다. cpptools는 이 프로젝트를 호스트
  IntelliSense 모드로 취급해 플랫폼 매크로를 정의하므로 spdlog가 ESP-IDF에 없는 `sys/syscall.h`나
  `windows.h`를 찾는다. shim은 그 매크로들을 IntelliSense에서만 undef한다. 빌드에는 관여하지 않으니
  지우거나 재생성하지 않는다.
- C++ 스타일은 `.clang-format`(LLVM 기반, 4칸 들여쓰기, Allman). 테스트·린터·CI는 없다.

## 구조

- `main/cardputer.cpp` — `app_main()`. `hal.init()` → mooncake DI에 `"HAL"` 등록 → HID OS 앱 하나만
  설치/실행 → `mooncake.update()` 무한 루프. 런처도, 메뉴도 없다.
- `main/apps/app_hid_os/` — 단일 앱. 부팅 시 `HID::Manager::begin()`으로 저장된 채널을 열고, 매 루프
  `updateKeyList()`/`updateKeysState()`로 키를 읽어 `fn` 콤보(채널 전환·재페어링·마우스 토글)를 처리한 뒤,
  마우스 모드면 이동/버튼을, 아니면 키보드 리포트를 보낸다. `hid_hud`가 상태 화면을 그린다.
- `main/hid/` — 트랜스포트와 영속화.
  - `hid_manager` — 채널 enum(USB/BT1/BT2/BT3)·상태·리포트 라우팅·채널 전환. 활성 채널은 항상 하나다.
  - `usb_hid` — TinyUSB 기반 USB HID. PHY를 OTG로 돌리는 `begin()`은 1회만 호출된다.
  - `ble_hid` — BLE HID 엔진(리포트맵: 키보드 id 1 + 마우스 id 2). 3슬롯 본딩/화이트리스트 광고를 관리한다.
  - `config_store` — `hid_os` namespace NVS. 본딩 키는 Bluedroid가, 슬롯↔주소 매핑은 여기서 관리.
  - `hid_keymap` — 물리키 문자열(`value_first`) 스캔 → 모디파이어/6키 배열/마우스 입력 변환.
  - `tinyusb_hid`, `usb_descriptors`, `tusb_config`, `esp_hid_gap` — 저수준 helper.
- `main/hal/` — 그대로 유지. 앱은 주변장치를 직접 만지지 않고
  `mcAppGetDatabase()->Get("HAL")->value<HAL::Hal*>()`로 받는다.
- `main/apps/utils/` — HAL이 의존하는 `boot_sound`·`common_define.h`와 HUD가 쓰는 `theme`만 남겼다.

## HID OS 규약

- 채널 전환은 재부팅 없이 런타임에 한다. 전환·마우스 토글 전에 항상 모든 키/버튼 release 리포트를 보낸다.
- 부팅 시 채널은 NVS에서 읽고, 마우스 모드는 항상 꺼진 상태로 시작한다.
- `fn`이 눌린 동안의 키는 OS 콤보로만 쓰고 호스트로 내보내지 않는다(키보드/마우스 리포트 금지).
- 리포트는 "디코드된 키 상태 해시"가 바뀔 때만 보낸다. 동일 리포트는 skip한다.
- 물리키 판정은 `Keyboard::keyList()`를 직접 스캔한다. `fn`은 `keysState().fn`을 봐도 된다.
- 상태 문구/색은 `HID::Status` 하나로 모은다. 트랜스포트는 `HID::Manager`를 통해서만 접근한다.
