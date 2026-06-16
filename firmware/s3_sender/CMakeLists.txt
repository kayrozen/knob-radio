# ESP32-S3 sender firmware (PCM -> COBS/UART forward link).
# Target: esp32s3 (R8, 8 MB PSRAM).
#
# Phases A-D build with plain ESP-IDF (this project). Phase E adds the ESP-ADF
# pipeline + LVGL UI; see docs/prototype-plan.md and README.
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../components")

# Phase E: if ESP-ADF is available (ADF_PATH exported), register its components.
# The default TONE build does not set ADF_PATH and skips this entirely.
if(DEFINED ENV{ADF_PATH})
    include($ENV{ADF_PATH}/CMakeLists.txt)
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(s3_sender)
