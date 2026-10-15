/*
 * IntelliSense-only shim (never compiled into the firmware).
 *
 * The C/C++ extension cannot be told "this is an Xtensa/ESP-IDF target": it
 * only knows <platform>-<compiler>-<arch> IntelliSense modes and, on a Linux
 * host, predefines the host macros below for every project whose compiler it
 * does not recognize (github.com/microsoft/vscode-cpptools/issues/4653).
 * xtensa-esp32s3-elf-gcc defines __XTENSA__/__ELF__ but never __linux__.
 *
 * The wrong __linux__ makes header-only spdlog (pulled in by <mooncake.h>)
 * take its Linux branch and #include <sys/syscall.h>, a glibc header that does
 * not exist in ESP-IDF's newlib - the "cannot open source file" squiggle.
 * Undefining the assumed host macros here restores what the real build sees.
 *
 * Wired up through C_Cpp.default.forcedInclude in ../settings.json, because
 * PlatformIO regenerates .vscode/c_cpp_properties.json on every project update.
 */
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
