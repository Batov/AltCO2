#include "ble_ess.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <errno.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(ble_ess, LOG_LEVEL_DBG);

#define BT_UUID_CO2 BT_UUID_DECLARE_16(0x2B8C)
#define BT_UUID_TEMP BT_UUID_DECLARE_16(0x2A6E)

static uint16_t co2_ppm;
static int16_t temperature_cdeg;
static uint16_t humidity_centi_pct;
static uint32_t pressure_deci_pa;
static char co2_str[32];
static struct k_work_delayable adv_restart_work;

#define ADV_RESTART_DELAY_MS 200
#define ADV_RESTART_RETRY_MS 1000

static ssize_t read_co2(struct bt_conn *conn,
                        const struct bt_gatt_attr *attr,
                        void *buf,
                        uint16_t len,
                        uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                             &co2_ppm, sizeof(co2_ppm));
}

static ssize_t read_temperature(struct bt_conn *conn,
                                const struct bt_gatt_attr *attr,
                                void *buf,
                                uint16_t len,
                                uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                             &temperature_cdeg, sizeof(temperature_cdeg));
}

static ssize_t read_humidity(struct bt_conn *conn,
                             const struct bt_gatt_attr *attr,
                             void *buf,
                             uint16_t len,
                             uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                             &humidity_centi_pct, sizeof(humidity_centi_pct));
}

static ssize_t read_pressure(struct bt_conn *conn,
                             const struct bt_gatt_attr *attr,
                             void *buf,
                             uint16_t len,
                             uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                             &pressure_deci_pa, sizeof(pressure_deci_pa));
}

static ssize_t read_co2_str(struct bt_conn *conn,
                            const struct bt_gatt_attr *attr,
                            void *buf,
                            uint16_t len,
                            uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                             co2_str, strlen(co2_str));
}

BT_GATT_SERVICE_DEFINE(ess_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_ESS),

    BT_GATT_CHARACTERISTIC(BT_UUID_CO2,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ,
                           read_co2, NULL, &co2_ppm),
    BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

    BT_GATT_CHARACTERISTIC(BT_UUID_TEMP,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ,
                           read_temperature, NULL, &temperature_cdeg),
    BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

    BT_GATT_CHARACTERISTIC(BT_UUID_HUMIDITY,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ,
                           read_humidity, NULL, &humidity_centi_pct),
    BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

    BT_GATT_CHARACTERISTIC(BT_UUID_PRESSURE,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ,
                           read_pressure, NULL, &pressure_deci_pa),
    BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(0x2901),
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ,
                           read_co2_str, NULL, co2_str),
    BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE)
);

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS,
                  BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
    BT_DATA_BYTES(BT_DATA_UUID16_ALL,
                  BT_UUID_16_ENCODE(0x181A)),
};

static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE,
            CONFIG_BT_DEVICE_NAME,
            sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static int start_advertising(void)
{
    int ret = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));

    if (ret == -EALREADY)
    {
        return 0;
    }

    return ret;
}

static void adv_restart_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    int ret = start_advertising();
    if (ret)
    {
        LOG_WRN("Advertising restart failed: %d, retry in %d ms",
                ret, ADV_RESTART_RETRY_MS);
        (void)k_work_reschedule(&adv_restart_work, K_MSEC(ADV_RESTART_RETRY_MS));
    }
    else
    {
        LOG_INF("Advertising restarted");
    }
}

static void connected(struct bt_conn *conn, uint8_t err)
{
    ARG_UNUSED(conn);

    if (err)
    {
        LOG_ERR("Connection failed: %d", err);
    }
    else
    {
        LOG_INF("Connected");
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    ARG_UNUSED(conn);
    LOG_INF("Disconnected, reason: %d", reason);

    (void)k_work_reschedule(&adv_restart_work, K_MSEC(ADV_RESTART_DELAY_MS));
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

int ble_ess_init(void)
{
    k_work_init_delayable(&adv_restart_work, adv_restart_work_handler);

    int ret = bt_enable(NULL);
    if (ret)
    {
        LOG_ERR("BT enable failed: %d", ret);
        return ret;
    }

    ret = start_advertising();
    if (ret)
    {
        LOG_ERR("Advertising start failed: %d", ret);
        return ret;
    }

    LOG_INF("BLE ESS advertising started");
    return 0;
}

void ble_ess_update_co2(uint16_t ppm)
{
    co2_ppm = ppm;
    bt_gatt_notify(NULL, &ess_svc.attrs[1], &co2_ppm, sizeof(co2_ppm));
}

void ble_ess_update_temperature(int16_t temp_cdeg)
{
    temperature_cdeg = temp_cdeg;
    bt_gatt_notify(NULL, &ess_svc.attrs[4], &temperature_cdeg, sizeof(temperature_cdeg));
}

void ble_ess_update_humidity(uint16_t humi_centi_pct)
{
    humidity_centi_pct = humi_centi_pct;
    bt_gatt_notify(NULL, &ess_svc.attrs[7], &humidity_centi_pct, sizeof(humidity_centi_pct));
}

void ble_ess_update_pressure(uint32_t press_deci_pa)
{
    pressure_deci_pa = press_deci_pa;
    bt_gatt_notify(NULL, &ess_svc.attrs[10], &pressure_deci_pa, sizeof(pressure_deci_pa));
}

void ble_ess_update_co2_str(uint16_t co2_value_ppm, int16_t temp_cdeg)
{
    snprintf(co2_str, sizeof(co2_str), "CO2:%dppm T:%d.%02dC",
             (int)co2_value_ppm,
             temp_cdeg / 100,
             abs(temp_cdeg % 100));

    bt_gatt_notify(NULL, &ess_svc.attrs[13], co2_str, strlen(co2_str));
}
