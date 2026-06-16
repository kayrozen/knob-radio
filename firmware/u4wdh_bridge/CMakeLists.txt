# ESP32-U4WDH bridge firmware (classic-BT A2DP source).
# Target: esp32 (LX6 dual-core, 4 MB flash, no PSRAM).
cmake_minimum_required(VERSION 3.16)

# Pull in the shared pcm_link protocol component from the repo root.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(u4wdh_bridge)
