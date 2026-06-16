# Base sources/requirements (the TONE build = plain ESP-IDF, phases B-D).
set(srcs
    "app_main.c"
    "pcm_source.c"
    "uart_writer.c"
    "backpressure_rx.c"
    "station.c")            # small, dependency-free; always compiled
set(reqs pcm_link driver nvs_flash esp_timer)

# IMPORTANT: SRCS may depend on Kconfig, but REQUIRES may NOT — component
# requirements are expanded BEFORE sdkconfig is loaded, so a CONFIG_* test here
# is always false during early dependency expansion and the required components'
# include dirs never propagate (the .c files then fail to find their headers).
# So: gate SRCS on Kconfig, but gate REQUIRES on conditions that are knowable
# during early expansion (env vars / always-present components).

# Phase E sources: the ESP-ADF pipeline, WiFi and the encoder.
if(CONFIG_PRESET_AUDIO_SOURCE_ADF)
    list(APPEND srcs "wifi_sta.c" "encoder.c" "adf_pipeline.c")
endif()

# Phase E requirements: the ESP-ADF components only exist when ADF_PATH is
# exported — the same gate the top-level CMakeLists uses to register them.
# esp_wifi/esp_netif/esp_event are base IDF but only the ADF build needs them,
# so they ride the same gate to keep the TONE build lean.
if(DEFINED ENV{ADF_PATH})
    list(APPEND reqs esp_wifi esp_netif esp_event
                     audio_pipeline audio_stream esp-adf-libs audio_sal esp_dispatcher)
endif()

# Optional LVGL UI on the round display. The LVGL/display managed components are
# auto-required via idf_component.yml; esp_lcd is base IDF. ui.c is only compiled
# when the UI is enabled, but (per the note above) its REQUIRES can't be
# Kconfig-gated, so esp_lcd is required unconditionally — negligible for the
# non-UI builds, which already pull the managed components anyway.
if(CONFIG_PRESET_ENABLE_UI)
    list(APPEND srcs "ui.c")
endif()
list(APPEND reqs esp_lcd esp_lvgl_port lvgl esp_lcd_st77916)

idf_component_register(SRCS ${srcs} INCLUDE_DIRS "." REQUIRES ${reqs})
