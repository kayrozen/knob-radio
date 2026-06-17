# Base sources/requirements (the TONE build = plain ESP-IDF, phases B-D).
# audio_output + dac_control are always compiled (the analog path is selected
# at boot, but both paths build everywhere so CI compile-checks the DAC code).
set(srcs
    "app_main.c"
    "pcm_source.c"
    "uart_writer.c"
    "backpressure_rx.c"
    "audio_output.c"
    "dac_control.c"
    "settings.c"
    "station.c")            # small, dependency-free; always compiled
set(reqs pcm_link driver nvs_flash esp_timer json)

# Phase 10: captive portal sources. Its REQUIRES (esp_http_server etc.) live in
# the ADF_PATH block below, because REQUIRES can't be Kconfig-gated (see note).
if(CONFIG_PRESET_ENABLE_PORTAL)
    list(APPEND srcs "portal.c")
endif()

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
# so they ride the same gate to keep the TONE build lean. The HTTP client/server
# + TLS for album art (Phase 9) and the captive portal (Phase 10) ride it too:
# both features only run in the ADF build, and their REQUIRES can't be
# Kconfig-gated.
if(DEFINED ENV{ADF_PATH})
    list(APPEND reqs esp_wifi esp_netif esp_event
                     audio_pipeline audio_stream esp-adf-libs audio_sal esp_dispatcher
                     esp_http_client esp-tls esp_http_server lwip)
endif()

# Optional LVGL UI on the round display. The LVGL/display managed components are
# auto-required via idf_component.yml; esp_lcd is base IDF. ui.c is only compiled
# when the UI is enabled, but (per the note above) its REQUIRES can't be
# Kconfig-gated, so esp_lcd is required unconditionally — negligible for the
# non-UI builds, which already pull the managed components anyway.
# The shared I2C bus is needed by the touch (UI) and/or the haptic.
set(need_i2c_bus OFF)

if(CONFIG_PRESET_ENABLE_UI)
    # display_st77916: panel bring-up + PWM backlight, used by ui.c.
    # touch_cst816: CST816S capacitive touch -> LVGL indev.
    # album_art: fetch the station favicon -> LVGL JPEG decode -> cover tile.
    list(APPEND srcs "ui.c" "display_st77916.c" "touch_cst816.c" "album_art.c")
    set(need_i2c_bus ON)
endif()

# Haptic (DRV2605) on the shared I2C bus.
if(CONFIG_PRESET_ENABLE_HAPTIC)
    list(APPEND srcs "haptic.c")
    set(need_i2c_bus ON)
endif()

if(need_i2c_bus)
    list(APPEND srcs "i2c_bus.c")
endif()

list(APPEND reqs esp_lcd esp_lvgl_port lvgl esp_lcd_st77916 esp_lcd_touch_cst816s)

idf_component_register(SRCS ${srcs} INCLUDE_DIRS "." REQUIRES ${reqs})
