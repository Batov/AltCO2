#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "sunrise.h"
#include "ble_ess.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

int main(void)
{
    LOG_INF("AltCO2 starting...");

    if (sunrise_init() != 0) {
        LOG_ERR("Sunrise init failed!");
        return -1;
    }

    if (ble_ess_init() != 0) {
        LOG_ERR("BLE init failed!");
        return -1;
    }

    sunrise_data_t data;
    while (1) {
        if (sunrise_read(&data) == 0) {
            LOG_INF("CO2: %d ppm | Temp: %d.%02d C",
                data.co2_ppm,
                data.temperature_cdeg / 100,
                abs(data.temperature_cdeg % 100));

            ble_ess_update_co2(data.co2_ppm);
            ble_ess_update_temperature(data.temperature_cdeg);
        }
        k_sleep(K_SECONDS(16));
    }

    return 0;
}