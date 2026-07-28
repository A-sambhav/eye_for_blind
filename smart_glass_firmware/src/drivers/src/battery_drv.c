#include <string.h>
#include "battery_drv.h"
#include "FreeRTOS.h"
#include "task.h"

#define BQ_I2C_ADDR 0x0B
#define REG_VOLTAGE 0x04
#define REG_CURRENT 0x0C
#define REG_TEMP    0x06
#define REG_SOC     0x1C
#define REG_CAPACITY 0x10
#define REG_FULL_CAP 0x0E
#define REG_CYCLES  0x1E
#define REG_STATUS  0x00
#define REG_ALERT   0x46

static struct {
    battery_drv_config_t config;
    bool initialized;
    uint32_t read_count;
} bat;

static int i2c_read_word(uint8_t addr, uint8_t reg, uint16_t *out)
{
    (void)addr;
    uint32_t t = xTaskGetTickCount() * portTICK_PERIOD_MS;
    switch (reg) {
        case REG_VOLTAGE:   *out = (uint16_t)(3900 + (t / 1000) % 200); break;
        case REG_CURRENT:   *out = (uint16_t)(-50 - (t / 500) % 100); break;
        case REG_TEMP:      *out = (uint16_t)(250 + (t / 2000) % 50); break;
        case REG_SOC:       *out = (uint16_t)(70 - (t / 60000) % 20); break;
        case REG_CAPACITY:  *out = 2500; break;
        case REG_FULL_CAP:  *out = 3000; break;
        case REG_CYCLES:    *out = 42; break;
        case REG_STATUS:    *out = (t / 10000) % 2 ? 0x0080 : 0x0000; break;
        default:            *out = 0; break;
    }
    return 0;
}

static int i2c_write_byte(uint8_t addr, uint8_t reg, uint8_t val)
{
    (void)addr; (void)reg; (void)val;
    return 0;
}

int battery_drv_init(const battery_drv_config_t *config)
{
    if (config == NULL) return -1;
    memset(&bat, 0, sizeof(bat));
    bat.config = *config;
    if (bat.config.i2c_addr == 0) bat.config.i2c_addr = BQ_I2C_ADDR;
    if (bat.config.i2c_baud == 0) bat.config.i2c_baud = 100000;
    bat.initialized = true;
    return 0;
}

int battery_drv_read_status(bq_status_t *out)
{
    if (!bat.initialized || out == NULL) return -1;
    memset(out, 0, sizeof(*out));

    uint16_t v, c, t, soc, rem, full, cycles, st;
    i2c_read_word(bat.config.i2c_addr, REG_VOLTAGE, &v);
    i2c_read_word(bat.config.i2c_addr, REG_CURRENT, &c);
    i2c_read_word(bat.config.i2c_addr, REG_TEMP, &t);
    i2c_read_word(bat.config.i2c_addr, REG_SOC, &soc);
    i2c_read_word(bat.config.i2c_addr, REG_CAPACITY, &rem);
    i2c_read_word(bat.config.i2c_addr, REG_FULL_CAP, &full);
    i2c_read_word(bat.config.i2c_addr, REG_CYCLES, &cycles);
    i2c_read_word(bat.config.i2c_addr, REG_STATUS, &st);

    out->voltage_uv = (uint32_t)v * 1000;
    out->current_ma = (int16_t)c;
    out->temp_c = (float)(int16_t)t * 0.1f;
    out->soc_pct = (uint8_t)(soc & 0xFF);
    out->soc_unfiltered_pct = out->soc_pct;
    out->charging_status = (st & 0x0080) ? 1 : ((st & 0x0040) ? 2 : 0);
    out->cycle_count = cycles;
    out->full_capacity_uah = (uint32_t)full * 1000;
    out->remaining_capacity_uah = (uint32_t)rem * 1000;
    out->fault_flags = st & 0xF000;
    bat.read_count++;
    return 0;
}

int battery_drv_set_alert_soc(uint8_t soc_pct)
{
    return i2c_write_byte(bat.config.i2c_addr, REG_ALERT, soc_pct);
}

int battery_drv_enter_shutdown(void)
{
    return i2c_write_byte(bat.config.i2c_addr, 0x00, 0x00);
}

int battery_drv_deinit(void)
{
    bat.initialized = false;
    return 0;
}
