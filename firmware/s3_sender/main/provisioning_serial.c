/*
 * provisioning_serial.c — see provisioning_serial.h.
 *
 * The browser installer sends a `PROVISION:{…}` line over USB. The console is
 * the USB-Serial-JTAG port (CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG), which already
 * installs that driver and wires stdio to it — so we must read the line from
 * stdin and reply on stdout rather than installing the driver a second time and
 * reading it directly (which would conflict with the console's stdio).
 */
#include "provisioning_serial.h"
#include "provision_apply.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#define PROVISION_PREFIX  "PROVISION:"
#define LINE_MAX          4096

static void respond(const char *s)
{
    fputs(s, stdout);
    fflush(stdout);
}

static void provision_task(void *arg)
{
    (void)arg;
    /* Unbuffered stdio so bytes arrive/leave as the installer sends/expects. */
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    char *line = malloc(LINE_MAX);
    if (!line) {
        vTaskDelete(NULL);
        return;
    }
    size_t len = 0;
    const size_t plen = strlen(PROVISION_PREFIX);

    for (;;) {
        int ch = fgetc(stdin);
        if (ch == EOF) {
            vTaskDelay(pdMS_TO_TICKS(20));   /* no data yet: yield and poll */
            continue;
        }
        if (ch == '\r') {
            continue;
        }
        if (ch != '\n') {
            if (len < LINE_MAX - 1) {
                line[len++] = (char)ch;
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
