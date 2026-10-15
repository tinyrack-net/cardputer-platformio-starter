
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
