
file(GLOB_RECURSE APPS_SRCS
    ./apps/*.c
    ./apps/*.cpp
)

file(GLOB_RECURSE HAL_SRCS
    ./hal/*.c
    ./hal/*.cpp
)

file(GLOB_RECURSE HID_SRCS
    ./hid/*.c
    ./hid/*.cpp
)

idf_component_register(SRCS "cardputer.cpp" ${APPS_SRCS} ${HAL_SRCS} ${HID_SRCS}
                    INCLUDE_DIRS "." "./hal" "./hid")


idf_component_get_property(tusb_lib leeebo__tinyusb_src COMPONENT_LIB)
cmake_policy(SET CMP0079 NEW)
target_link_libraries(${tusb_lib} PRIVATE ${COMPONENT_LIB})

# Intercept the BLE HID event-loop task creation so it can be given a larger
# stack than the 2048 bytes IDF 4.4.6 hard-codes (see esp_hid_stack_fix.c).
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_event_loop_create")
