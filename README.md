# AltCO2 Firmware

Прошивка для CO2-монитора на базе nRF52832 (Zephyr RTOS / nRF Connect SDK v3.2.4).

## Железо

- **MCU:** Nordic nRF52832
- **CO2:** Senseair Sunrise 006-0-0007 (I2C1, SCL=P0.15, SDA=P0.16)

## Что работает

- [x] Чтение CO2 и температуры с Senseair Sunrise
- [x] BLE Environmental Sensing Service (ESS, UUID 0x181A)
- [x] GATT notify каждые 16 секунд

## BLE характеристики

| Характеристика | UUID   | Формат | Единица |
|---|---|---|---|
| CO2 | 0x2B8C | uint16 LE | ppm |
| Температура | 0x2A6E | int16 LE | 0.01 °C |

## Сборка

```bash
# Установить nRF Connect SDK v3.2.4
# Открыть папку в VS Code с расширением nRF Connect
# Выбрать плату: nrf52dk/nrf52832
west build -b nrf52dk/nrf52832
west flash
```

## Отладка

RTT через J-Link. Открыть J-Link RTT Viewer:
- Target device: `NRF52832_XXAA_AA`
- Interface: SWD
- Speed: 4000 kHz
