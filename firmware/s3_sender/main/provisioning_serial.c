/*
 * provisioning_serial.c — see provisioning_serial.h.
 */
#include "provisioning_serial.h"
#include "provision_apply.h"

#include <string.h>
#include <stdlib.h>

#include "driver/usb_serial_jtag.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#define PROVISION_PREFIX  "PROVISION:"
#define LINE_MAX          4096

static void respond(const char *s)
{
    usb_serial_jtag_write_bytes((const uint8_t *)s, strlen(s), pdMS_TO_TICKS(200));
}

static void provision_task(void *arg)
{
    (void)arg;
    usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    usb_serial_jtag_driver_install(&cfg);

    char *line = malloc(LINE_MAX);
    if (!line) {
        vTaskDelete(NULL);
        return;
    }
    size_t len = 0;
    const size_t plen = strlen(PROVISION_PREFIX);

    for (;;) {
        uint8_t c;
        if (usb_serial_jtag_read_bytes(&c, 1, portMAX_DELAY) != 1) {
            continue;
        }
        if (c == '\r') {
            continue;
        }
        if (c != '\n') {
            if (len < LINE_MAX - 1) {
                line[len++] = (char)c;
            } else {
                len = 0;   /* overrun: drop the line */
            }
            continue;
        }

        line[len] = '\0';
        if (len > plen && strncmp(line, PROVISION_PREFIX, plen) == 0) {
            if (provision_apply_json(line + plen)) {
                respond("OK\n");
                vTaskDelay(pdMS_TO_TICKS(400));   /* let the reply drain */
                esp_restart();
            } else {
                respond("ERR:invalid provisioning payload\n");
            }
        }
        len = 0;
    }
}

void provisioning_serial_start(void)
{
    xTaskCreate(provision_task, "provision", 6144, NULL, 5, NULL);
}
