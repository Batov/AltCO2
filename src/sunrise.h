#pragma once

#include <stdint.h>

typedef struct {
    uint16_t co2_ppm;
    int16_t  temperature_cdeg; /* температура в сотых градуса, например 2350 = 23.50 C */
} sunrise_data_t;

int sunrise_init(void);
int sunrise_read(sunrise_data_t *data);