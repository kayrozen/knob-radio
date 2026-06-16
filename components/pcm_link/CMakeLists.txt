# Shared PCM-over-UART link component, used by both firmware projects.
# Pure C, no ESP-IDF dependencies, so the same sources also compile on the
# host toolchain for unit tests (see test/host).
idf_component_register(
    SRCS
        "src/cobs.c"
        "src/pcm_frame.c"
        "src/pcm_link_rx.c"
        "src/control_msg.c"
    INCLUDE_DIRS
        "include"
)
