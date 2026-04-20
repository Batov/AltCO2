#include "sunrise.h"

#include <stdlib.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sunrise, LOG_LEVEL_DBG);

#define REG_ERROR_STATUS 0x01
#define REG_CO2_FILTERED 0x06

#define WAKEUP_TIME_MS 35
#define NRDY_TIMEOUT_MS 2000

#define CO2_EN_PIN 26
#define CO2_NRDY_PIN 27

#define SUNRISE_ADDR 0x68

static const struct device *i2c_dev;
static const struct device *gpio_dev;

static int sunrise_wakeup(void)
{
    uint8_t dummy = REG_CO2_FILTERED;
    int ret = i2c_write(i2c_dev, &dummy, 1, SUNRISE_ADDR);

    /* First write may NACK while sensor wakes up. */
    if (ret && ret != -EIO)
    {
        LOG_DBG("Wakeup write returned: %d", ret);
    }

    k_sleep(K_MSEC(5));
    return 0;
}

static int sunrise_read_reg16(uint8_t reg, uint16_t *value)
{
    uint8_t buf[2];
    int ret = i2c_write_read(i2c_dev, SUNRISE_ADDR, &reg, 1, buf, 2);

    if (ret)
    {
        LOG_ERR("Read reg 0x%02x failed: %d", reg, ret);
        return ret;
    }

    *value = (buf[0] << 8) | buf[1];
    return 0;
}

static int wait_nrdy(void)
{
    int timeout = NRDY_TIMEOUT_MS;

    while (gpio_pin_get(gpio_dev, CO2_NRDY_PIN) != 0)
    {
        k_sleep(K_MSEC(10));
        timeout -= 10;

        if (timeout <= 0)
        {
            LOG_WRN("nRDY timeout");
            return -ETIMEDOUT;
        }
    }

    return 0;
}

int sunrise_init(void)
{
    uint16_t errors;

    i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
    gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));

    if (!device_is_ready(i2c_dev))
    {
        LOG_ERR("I2C not ready");
        return -ENODEV;
    }

    if (!device_is_ready(gpio_dev))
    {
        LOG_ERR("GPIO not ready");
        return -ENODEV;
    }

    int ret = gpio_pin_configure(gpio_dev, CO2_EN_PIN, GPIO_OUTPUT_ACTIVE);
    if (ret)
    {
        LOG_ERR("CO2_EN pin configure failed: %d", ret);
        return ret;
    }

    ret = gpio_pin_set(gpio_dev, CO2_EN_PIN, 1);
    if (ret)
    {
        LOG_ERR("CO2_EN pin set failed: %d", ret);
        return ret;
    }

    ret = gpio_pin_configure(gpio_dev, CO2_NRDY_PIN, GPIO_INPUT);
    if (ret)
    {
        LOG_ERR("CO2_NRDY pin configure failed: %d", ret);
        return ret;
    }

    k_sleep(K_MSEC(WAKEUP_TIME_MS));

    sunrise_wakeup();
    ret = sunrise_read_reg16(REG_ERROR_STATUS, &errors);
    if (ret)
    {
        LOG_ERR("Sunrise not responding");
        return ret;
    }

    if (errors)
    {
        LOG_WRN("Sunrise error status: 0x%04x (may clear after first measurement)", errors);

        /* Bit 15 is expected at startup and can be ignored. */
        if (errors & ~0x8000)
        {
            LOG_ERR("Critical errors present");
            return -EIO;
        }
    }
    else
    {
        LOG_INF("Sunrise OK, no errors");
    }

    return 0;
}

int sunrise_read(sunrise_data_t *data)
{
    if (data == NULL)
    {
        return -EINVAL;
    }

    int ret = wait_nrdy();
    if (ret)
    {
        return ret;
    }

    sunrise_wakeup();

    uint8_t reg = REG_CO2_FILTERED;
    uint8_t buf[4];

    ret = i2c_write_read(i2c_dev, SUNRISE_ADDR, &reg, 1, buf, 4);
    if (ret)
    {
        LOG_ERR("Read failed: %d", ret);
        return ret;
    }

    data->co2_ppm = (buf[0] << 8) | buf[1];
    data->temperature_cdeg = (int16_t)((buf[2] << 8) | buf[3]);

    LOG_DBG("CO2: %d ppm, temp: %d.%02d C",
            data->co2_ppm,
            data->temperature_cdeg / 100,
            abs(data->temperature_cdeg % 100));

    return 0;
}
