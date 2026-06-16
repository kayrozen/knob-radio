/*
 * i2c_bus.c — see i2c_bus.h.
 */
#include "i2c_bus.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"

static const char *TAG = "i2c_bus";

static i2c_master_bus_handle_t s_bus;
static SemaphoreHandle_t       s_lock;

esp_err_t i2c_bus_init(void)
{
    if (s_bus) {
        return ESP_OK;   /* already up */
    }

    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) {
        return ESP_ERR_NO_MEM;
    }

    const i2c_master_bus_config_t cfg = {
        .clk_source                   = I2C_CLK_SRC_DEFAULT,
        .i2c_port                     = I2C_NUM_0,
        .scl_io_num                   = I2C_BUS_SCL_GPIO,
        .sda_io_num                   = I2C_BUS_SDA_GPIO,
        .glitch_ignore_cnt            = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&cfg, &s_bus);
    if (err != ESP_OK) {
        vSemaphoreDelete(s_lock);
        s_lock = NULL;
        ESP_LOGE(TAG, "i2c_new_master_bus failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "shared I2C up: SDA=%d SCL=%d @ %d Hz",
             I2C_BUS_SDA_GPIO, I2C_BUS_SCL_GPIO, I2C_BUS_FREQ_HZ);
    return ESP_OK;
}

i2c_master_bus_handle_t i2c_bus_handle(void)
{
    return s_bus;
}

void i2c_bus_lock(void)
{
    if (s_lock) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
    }
}

void i2c_bus_unlock(void)
{
    if (s_lock) {
        xSemaphoreGive(s_lock);
    }
}

/* Add a device, run `body`, remove it — all under the bus lock. */
static esp_err_t with_device(uint8_t addr, esp_err_t (*body)(i2c_master_dev_handle_t, void *),
                             void *arg)
{
    if (!s_bus) {
        return ESP_ERR_INVALID_STATE;
    }
    const i2c_device_config_t dcfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = addr,
        .scl_speed_hz    = I2C_BUS_FREQ_HZ,
    };

    i2c_bus_lock();
    i2c_master_dev_handle_t dev = NULL;
    esp_err_t err = i2c_master_bus_add_device(s_bus, &dcfg, &dev);
    if (err == ESP_OK) {
        err = body(dev, arg);
        i2c_master_bus_rm_device(dev);
    }
    i2c_bus_unlock();
    return err;
}

struct buf_arg { const uint8_t *data; size_t len; uint8_t *out; };

static esp_err_t do_write(i2c_master_dev_handle_t dev, void *arg)
{
    struct buf_arg *a = (struct buf_arg *)arg;
    return i2c_master_transmit(dev, a->data, a->len, -1);
}

static esp_err_t do_read_reg(i2c_master_dev_handle_t dev, void *arg)
{
    struct buf_arg *a = (struct buf_arg *)arg;
    return i2c_master_transmit_receive(dev, a->data, 1, a->out, 1, -1);
}

esp_err_t i2c_bus_write(uint8_t addr, const uint8_t *data, size_t len)
{
    struct buf_arg a = { .data = data, .len = len, .out = NULL };
    return with_device(addr, do_write, &a);
}

esp_err_t i2c_bus_write_reg(uint8_t addr, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return i2c_bus_write(addr, buf, sizeof(buf));
}

esp_err_t i2c_bus_read_reg(uint8_t addr, uint8_t reg, uint8_t *val)
{
    struct buf_arg a = { .data = &reg, .len = 1, .out = val };
    return with_device(addr, do_read_reg, &a);
}
