/*
 * IntelliSense-only shim (never compiled into the firmware).
 *
 * The C/C++ extension cannot be told "this is an Xtensa/ESP-IDF target": it
 * only knows <platform>-<compiler>-<arch> IntelliSense modes, so a compiler it
 * cannot classify (xtensa-esp32s3-elf-gcc) makes it fall back to the *host*
 * mode and predefine that host's macros for the whole project
 * (github.com/microsoft/vscode-cpptools/issues/4653). The Xtensa toolchain
 * defines __XTENSA__/__ELF__ but none of the macros below.
 *
 * Those wrong platform macros change which branch host-aware libraries take:
 * header-only spdlog (pulled in by <mooncake.h>) includes <sys/syscall.h> under
 * __linux__ and <windows.h> under _WIN32, and ESP-IDF's newlib ships neither -
 * the "cannot open source file" squiggle in every app source, while `pio run`
 * compiles fine. Undefining the assumed host macros here restores what the real
 * build sees, on a Windows or a POSIX host.
 *
 * Wired up through C_Cpp.default.forcedInclude in ../settings.json, because
 * PlatformIO regenerates .vscode/c_cpp_properties.json on every project update.
 */

/* POSIX host, e.g. the linux-gcc-x64 / macos-clang-x64 IntelliSense modes */
#undef __linux__
#undef linux
#undef __gnu_linux__
#undef __unix
#undef __unix__
#undef unix
#undef __i386
#undef __i386__
#undef __x86_64
#undef __x86_64__
#undef __APPLE__
#undef __MACH__

/* Windows host, e.g. the windows-msvc-x64 IntelliSense mode */
#undef _WIN32
#undef _WIN64
#undef _WINDOWS
#undef _M_IX86
#undef _M_X64
#undef _M_AMD64
#undef _MSC_VER
#undef _MSC_FULL_VER
#undef _MSC_BUILD
#undef _MSC_EXTENSIONS
