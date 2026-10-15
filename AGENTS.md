# cardputer

M5Stack Cardputer용 M5Cardputer-UserDemo(mooncake 앱 프레임워크 + 자체 HAL)를 **PlatformIO + ESP-IDF
v4.4.6**으로 이전한 프로젝트. Arduino가 아니다. 문서는 `README.md`와 이 파일만 유지한다.

## 빌드 · 실행

PlatformIO Core는 PATH에 없고 VS Code 확장 설치 경로에 있다.

```bash
PIO=$HOME/.platformio/penv/bin/pio

"$PIO" run                                        # 빌드
"$PIO" run -t upload --upload-port /dev/ttyACM0   # 플래시 (앱 부팅까지 자동)
"$PIO" run -t appboot                             # 플래시 없이 다운로드 모드 탈출/앱 재시작
./flash.sh                                        # upload + serial monitor (대화형 터미널에서 실행)
"$PIO" device monitor --port /dev/ttyACM0         # 터미널 필요, 파이프/리다이렉트 불가
```

- 보드는 내장 USB-Serial/JTAG(`/dev/ttyACM0`, WSL은 usbipd 패스스루)로 연결한다. 최초 1회 `sudo ./hotplugSetup.sh`
  (시리얼 + USB-JTAG udev 규칙).
- esptool의 기본 리셋 때문에 업로드 후 칩이 다운로드 모드(화면 까맣)에 남는다. `scripts/pio_app_boot.py`가
  업로드 후 자동으로 앱을 부팅시키고 부팅 로그를 출력하므로, 업로드 성공 여부는 그 로그로 판단한다.
  그래도 화면이 까맣면 `pio run -t appboot`, 그다음은 전원 재인가.
- JTAG 디버깅(`pio debug`)은 `platformio/tool-openocd-esp32` 설치 + 위 udev 규칙이 있어야 한다(없으면
  `LIBUSB_ERROR_ACCESS`).
- 첫 빌드는 ESP-IDF 툴체인과 관리 컴포넌트를 내려받는다(네트워크 필요, 수 분). Python 3.13+ 호스트는
  ESP-IDF용 가상환경 `~/.platformio/penv/.espidf-4.4.6`을 3.12 이하로 만들어야 한다 — 명령은 `README.md`
  "Environment notes" 참고.

## 지켜야 할 것

- `platformio.ini`의 버전 고정을 올리지 않는다: `espressif32@6.6.0` + `framework-espidf@~3.40406.0`(IDF 4.4.6)
  + `toolchain-xtensa-esp32s3@8.4.0`. 최신 플랫폼은 GCC 13을 붙여 ESP-IDF 4.4를 컴파일하지 못한다.
- 보드 설정(Kconfig)은 `sdkconfig.defaults`로만 바꾼다. 생성물 `sdkconfig.m5stack-cardputer`는 커밋하지
  않으며, 수정 후에는 지우고 리빌드한다.
- 의존성은 `main/idf_component.yml`에 선언하고 `managed_components/`는 커밋하지 않는다.
  `components/infrared_tools`만 벤더링 유지(레지스트리 미등록).
- 업스트림 소스 수정은 최소화한다. 현재 차이는 `main/hal/hal.h`의 `#include <M5GFX.h>` 한 곳뿐이다.
- `[platformio] src_dir = main` → `main/`이 ESP-IDF 메인 컴포넌트이고 `main/CMakeLists.txt`가 그대로 쓰인다
  (glob 방식이라 앱 폴더를 추가해도 CMake 수정이 필요 없다).
- IntelliSense용 `.vscode/settings.json`(`C_Cpp.default.forcedInclude`)과
  `.vscode/intellisense/esp_idf_undef_host_macros.h`는 의도된 설정이다. cpptools가 이 프로젝트를 호스트
  `linux-gcc-x64` 모드로 취급해 `__linux__`를 정의하므로 spdlog가 ESP-IDF에 없는 `sys/syscall.h`를
  찾는다. 빌드에는 관여하지 않으니 지우거나 재생성하지 않는다.
- C++ 스타일은 `.clang-format`(LLVM 기반, 4칸 들여쓰기, Allman). 테스트·린터·CI는 없다.

## 구조

- `main/cardputer.cpp` — `app_main()`. `hal.init()` → mooncake DI에 `"HAL"` 등록 → 앱별 `installApp()` →
  런처 설치 → `mooncake.update()` 무한 루프. `ON_HAL_TEST`/`ON_APP_TEST*` 블록은 개발용이므로 주석 상태 유지.
- `main/hal/` — `HalCardputer`(display/keyboard/mic/speaker/home button/battery/sdcard). 앱은 주변장치를 직접
  만지지 않고 `mcAppGetDatabase()->Get("HAL")->value<HAL::Hal*>()`로 받는다.
- `main/apps/app_*` — 앱 하나는 `APP_BASE`(인스턴스·생명주기) + `APP_PACKER_BASE`(이름/아이콘/팩토리) 쌍으로
  만들고, 아이콘은 `apps/utils/`, 색·폰트는 `apps/utils/theme/` 규약을 따른다.
- 런처 메뉴는 설치된 앱 목록에서 자동 생성된다. 앱 등록은 `cardputer.cpp`의 `installApp()` 한 줄이 전부이고
  메뉴 쪽 파일을 고칠 필요가 없다.
