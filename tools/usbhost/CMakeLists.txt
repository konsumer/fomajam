# The FOMAJAM USB MIDI host box (CC.md): tools/usbhost/build.sh makes build/fomajam-usbhost.uf2
cmake_minimum_required(VERSION 3.13)
set(PICO_BOARD adafruit_feather_rp2040_usb_host CACHE STRING "")
set(PICO_PIO_USB_PATH $ENV{PICO_PIO_USB_PATH}) # (TinyUSB builds it in, for its USB-A socket)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
project(fomajam_usbhost C CXX ASM)
pico_sdk_init()

add_executable(fomajam-usbhost main.c)
target_include_directories(fomajam-usbhost PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(fomajam-usbhost PRIVATE pico_stdlib tinyusb_host tinyusb_pico_pio_usb)
pico_add_extra_outputs(fomajam-usbhost)
