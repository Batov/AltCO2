#pragma once

#include <stdint.h>

int ble_ess_init(void);
void ble_ess_update_co2(uint16_t co2_ppm);
void ble_ess_update_temperature(int16_t temperature_cdeg);
void ble_ess_update_humidity(uint16_t humidity_centi_pct);
void ble_ess_update_pressure(uint32_t pressure_deci_pa);