/*
 * power_sleep.c — see power_sleep.h.
 */
#include "power_sleep.h"
#include "uart_writer.h"
#include "backpressure_rx.h"
#include "pcm_link_proto.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "power_sleep";

#ifndef CONFIG_PRESET_SLEEP_AFTER_S
#define CONFIG_PRESET_SLEEP_AFTER_S  60
#endif
#ifndef CONFIG_PRESET_SLEEP_POLL_S
#define CONFIG_PRESET_SLEEP_POLL_S   5
#endif

#define POLL_WAIT_MS   1500   /* how long to wait for the U4WDH's status reply */

/* Survives deep sleep (RTC slow memory): set when we sleep for a gone car, so
 * the next timer wake knows to poll rather than do a normal boot. */
static RTC_DATA_ATTR bool s_slept_for_car;

static esp_timer_handle_t s_idle_timer;          /* no-car inactivity countdown */
static bool               s_ever_connected;      /* a car connected this session  */
static volatile bool      s_poll_replied;        /* U4WDH answered the status req */
static volatile bool      s_poll_connected;      /* ...and the car is connected   */

/* Configure the RTC timer wake and drop into deep sleep. Never returns. */
static void enter_deep_sleep(void)
{
    s_slept_for_car = true;
    ESP_LOGI(TAG, "no car for %ds -> deep sleep, polling every %ds",
             CONFIG_PRESET_SLEEP_AFTER_S, CONFIG_PRESET_SLEEP_POLL_S);
    esp_sleep_enable_timer_wakeup((uint64_t)CONFIG_PRESET_SLEEP_POLL_S * 1000000ULL);
    esp_deep_sleep_start();
}

static void idle_timer_cb(void *arg)
{
    (void)arg;
    enter_deep_sleep();   /* never returns */
}

static void arm_idle_timer(void)
{
    if (!s_idle_timer) {
        const esp_timer_create_args_t a = { .callback = idle_timer_cb, .name = "no_car" };
        esp_timer_create(&a, &s_idle_timer);
    }
    esp_timer_stop(s_idle_timer);   /* harmless if not running */
    esp_timer_start_once(s_idle_timer,
                         (uint64_t)CONFIG_PRESET_SLEEP_AFTER_S * 1000000ULL);
}

/* Minimal BT-status callback used only during the deep-sleep wake poll. */
static void poll_bt_cb(uint8_t state)
{
    s_poll_connected = (state == PCM_LINK_BT_CONNECTED);
    s_poll_replied   = true;
}

void power_sleep_boot_hook(void)
{
    if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_TIMER || !s_slept_for_car) {
        return;   /* normal cold boot */
    }

    /* Bring up just the UART link + return listener and ask whether the car is
     * back. Nothing else (no WiFi/ADF/UI/haptic) so a poll is cheap and silent. */
    s_poll_replied = false;
    s_poll_connected = false;
    uart_writer_init_link();
    backpressure_rx_set_handlers(NULL, poll_bt_cb);
    backpressure_rx_start();
    uart_writer_send_control(PCM_LINK_CTRL_BT_STATUS_REQ, NULL, 0);

    /* Wake only long enough to hear back (then sleep again if no car). */
    for (int waited = 0; waited < POLL_WAIT_MS && !s_poll_replied; waited += 25) {
        vTaskDelay(pdMS_TO_TICKS(25));
    }

    if (s_poll_connected) {
        s_slept_for_car = false;
        ESP_LOGI(TAG, "car is back -> full boot");
        esp_restart();          /* clean full boot (link re-inits from scratch) */
    }
    enter_deep_sleep();         /* still gone -> back to sleep; never returns */
}

void power_sleep_init(void)
{
    /* Don't arm anything yet: a device that has never seen a car (e.g. sitting
     * at home for setup over the LAN editor) must stay awake and reachable. We
     * only sleep once a car has connected and then gone (the engine-off signal).
     * A car already connected at boot won't re-emit its state, so ask for it. */
    uart_writer_send_control(PCM_LINK_CTRL_BT_STATUS_REQ, NULL, 0);
}

void power_sleep_on_bt(uint8_t bt_state)
{
    if (bt_state == PCM_LINK_BT_CONNECTED) {
        s_ever_connected = true;
        if (s_idle_timer) {
            esp_timer_stop(s_idle_timer);   /* car present: stay awake */
        }
    } else if (bt_state == PCM_LINK_BT_DISCONNECTED && s_ever_connected) {
        arm_idle_timer();                   /* car was here and left: count down */
    }
}

void power_sleep_kick(void)
{
    if (s_idle_timer && esp_timer_is_active(s_idle_timer)) {
        arm_idle_timer();   /* user is interacting: push the countdown back */
    }
}
