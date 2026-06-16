# Base sources/requirements (the TONE build = plain ESP-IDF, phases B-D).
set(srcs
    "app_main.c"
    "pcm_source.c"
    "uart_writer.c"
    "backpressure_rx.c"
    "station.c")            # small, dependency-free; always compiled
set(reqs pcm_link driver nvs_flash esp_timer)

# Phase E: ESP-ADF internet-radio pipeline + WiFi + encoder.
if(CONFIG_PRESET_AUDIO_SOURCE_ADF)
    list(APPEND srcs "wifi_sta.c" "encoder.c" "adf_pipeline.c")
    list(APPEND reqs esp_wifi esp_netif esp_event
                     audio_pipeline audio_stream esp-adf-libs audio_sal esp_dispatcher)
endif()

# Optional LVGL UI on the round display (managed components, see idf_component.yml).
if(CONFIG_PRESET_ENABLE_UI)
    list(APPEND srcs "ui.c")
    list(APPEND reqs esp_lcd esp_lvgl_port lvgl esp_lcd_st77916)
endif()

idf_component_register(SRCS ${srcs} INCLUDE_DIRS "." REQUIRES ${reqs})
