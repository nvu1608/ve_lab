#include "ds1307_sim.h"
#include <string.h>

/* ============================================================
 * DS1307 Datasheet Definitions
 * ============================================================ */
#define DS1307_I2C_ADDR      0x68u

#define DS1307_REG_SECONDS   0x00u
#define DS1307_REG_MINUTES   0x01u
#define DS1307_REG_HOURS     0x02u
#define DS1307_REG_DAY       0x03u
#define DS1307_REG_DATE      0x04u
#define DS1307_REG_MONTH     0x05u
#define DS1307_REG_YEAR      0x06u
#define DS1307_REG_CONTROL   0x07u
#define DS1307_REG_RAM_BASE  0x08u

#define DS1307_BIT_CH        (1u << 7)
#define DS1307_BIT_12H       (1u << 6)
#define DS1307_BIT_PM        (1u << 5)

#define DS1307_MASK_SECONDS  0x7Fu
#define DS1307_MASK_MINUTES  0x7Fu
#define DS1307_MASK_HOURS_12 0x1Fu
#define DS1307_MASK_HOURS_24 0x3Fu
#define DS1307_MASK_DAY      0x07u
#define DS1307_MASK_DATE     0x3Fu
#define DS1307_MASK_MONTH    0x1Fu

#include "driver_i2c_slave.h"
#include "driver_gpio.h"
#include "driver_rcc.h"
#include "driver_nvic.h"


/* ============================================================
 * Private Helper Prototypes
 * ============================================================ */
static uint8_t prv_bin_to_bcd(uint8_t val);
static uint8_t prv_bcd_to_bin(uint8_t val);
static uint8_t prv_days_in_month(uint8_t month, uint16_t year);
static void prv_normalize_time(ds1307_time_t *time);

static void prv_sync_time_from_regs(ds1307_t *dev);
static void prv_sync_regs_from_time(ds1307_t *dev);
static void prv_publish_regs(ds1307_t *dev);

static void prv_i2c_event_handler(void *ctx, const i2c_slave_event_t *evt);
static uint8_t prv_i2c_tx_handler(void *ctx);
static void prv_sim_task(void *argument);

/* ============================================================
 * Public API Implementation (ds1307_api.h)
 * ============================================================ */

void ds1307_set_time(ds1307_t *dev, const ds1307_time_t *time) {
    if (dev == NULL || time == NULL) return;

    if (xSemaphoreTake(dev->mutex, portMAX_DELAY) == pdTRUE) {
        dev->current_time = *time;
        prv_normalize_time(&dev->current_time);
        prv_sync_regs_from_time(dev);
        prv_publish_regs(dev);
        xSemaphoreGive(dev->mutex);
    }
}

void ds1307_get_time(ds1307_t *dev, ds1307_time_t *time) {
    if (dev == NULL || time == NULL) return;

    if (xSemaphoreTake(dev->mutex, portMAX_DELAY) == pdTRUE) {
        *time = dev->current_time;
        xSemaphoreGive(dev->mutex);
    }
}

uint8_t ds1307_is_halted(ds1307_t *dev) {
    if (dev == NULL) return 1;
    return (dev->regs[DS1307_REG_SECONDS] & DS1307_BIT_CH) ? 1 : 0;
}

/* ============================================================
 * Simulator API Implementation (ds1307_sim.h)
 * ============================================================ */

void ds1307_sim_start(ds1307_t *dev, void *i2c_slave, uint32_t task_priority, uint32_t stack_size) {
    if (dev == NULL || i2c_slave == NULL) return;

    memset(dev, 0, sizeof(ds1307_t));
    dev->i2c_slave = i2c_slave;

    /* Initial Time: 00:00:00 01/01/2000 */
    dev->current_time.date = 1;
    dev->current_time.month = 1;
    dev->current_time.year = 2000;
    prv_sync_regs_from_time(dev);
    prv_publish_regs(dev);

    /* Create RTOS objects */
    dev->mutex = xSemaphoreCreateMutex();
    dev->write_sem = xSemaphoreCreateBinary();

    /* Register I2C Callbacks */
    i2c_slave_set_event_callback((i2c_slave_t *)dev->i2c_slave, prv_i2c_event_handler, dev);
    i2c_slave_set_tx_byte_callback((i2c_slave_t *)dev->i2c_slave, prv_i2c_tx_handler, dev);

    /* Create Simulation Task */
    xTaskCreate(prv_sim_task, "DS1307_Sim", (uint16_t)stack_size, dev, task_priority, &dev->task_handle);

    /* Start I2C Driver */
    i2c_slave_start((i2c_slave_t *)dev->i2c_slave);
}

/* ============================================================
 * Private Helper Implementation
 * ============================================================ */

static void prv_sim_task(void *argument) {  // logic simu ds1307 tick time 
    ds1307_t *dev = (ds1307_t *)argument;
    TickType_t last_tick = xTaskGetTickCount();

    while (1) {
        /* Wait for either 1s timeout or an I2C write event */
        if (xSemaphoreTake(dev->write_sem, pdMS_TO_TICKS(1000)) == pdTRUE) {
            /* Handle I2C write transaction */
            if (xSemaphoreTake(dev->mutex, portMAX_DELAY) == pdTRUE) {
                uint8_t reg = dev->write_start_reg;
                for (uint8_t i = 0; i < dev->write_len; i++) {
                    dev->regs[reg] = dev->write_buf[i];
                    reg = (uint8_t)((reg + 1) % DS1307_REG_COUNT);
                }

                if (dev->write_start_reg <= DS1307_REG_YEAR) {
                    prv_sync_time_from_regs(dev);
                }

                dev->write_pending = 0;
                dev->write_len = 0;
                prv_publish_regs(dev);
                xSemaphoreGive(dev->mutex);
            }
        } else {
            /* 1 second tick */
            if (xSemaphoreTake(dev->mutex, portMAX_DELAY) == pdTRUE) {
                if (!ds1307_is_halted(dev)) {
                    ds1307_time_t *t = &dev->current_time;
                    t->second++;
                    if (t->second >= 60) {
                        t->second = 0; t->minute++;
                        if (t->minute >= 60) {
                            t->minute = 0; t->hour++;
                            if (t->hour >= 24) {
                                t->hour = 0; t->day_of_week++;
                                if (t->day_of_week > 7) t->day_of_week = 1;
                                t->date++;
                                if (t->date > prv_days_in_month(t->month, t->year)) {
                                    t->date = 1; t->month++;
                                    if (t->month > 12) {
                                        t->month = 1; t->year++;
                                    }
                                }
                            }
                        }
                    }
                    prv_sync_regs_from_time(dev);
                    prv_publish_regs(dev);
                }
                xSemaphoreGive(dev->mutex);
            }
        }
    }
}

static void prv_i2c_event_handler(void *ctx, const i2c_slave_event_t *evt) {
    ds1307_t *dev = (ds1307_t *)ctx;
    BaseType_t woken = pdFALSE;

    switch (evt->type) {
        case I2C_SLAVE_EVT_WRITE_START:
            dev->write_len = 0;
            dev->write_pending = 0;
            break;

        case I2C_SLAVE_EVT_BYTE_RECEIVED:
            if (dev->write_pending == 0 && dev->write_len == 0) {
                dev->current_reg = evt->data % DS1307_REG_COUNT;
                dev->write_start_reg = dev->current_reg;
                dev->write_pending = 1; /* First byte is address pointer */
            } else {
                if (dev->write_len < DS1307_REG_COUNT) {
                    dev->write_buf[dev->write_len++] = evt->data;
                }
            }
            break;

        case I2C_SLAVE_EVT_WRITE_DONE:
            if (dev->write_len > 0) {
                xSemaphoreGiveFromISR(dev->write_sem, &woken);
            }
            break;

        case I2C_SLAVE_EVT_READ_START:
            dev->tx_bank = dev->active_bank;
            dev->tx_active = 1;
            break;

        case I2C_SLAVE_EVT_READ_DONE:
            dev->tx_active = 0;
            break;

        default: break;
    }
    portYIELD_FROM_ISR(woken);
}

static uint8_t prv_i2c_tx_handler(void *ctx) {
    ds1307_t *dev = (ds1307_t *)ctx;
    uint8_t data = dev->reg_bank[dev->tx_bank][dev->current_reg];
    dev->current_reg = (uint8_t)((dev->current_reg + 1) % DS1307_REG_COUNT);
    return data;
}

/* --- Core Logic Helpers (Simplified from old code) --- */

static uint8_t prv_bin_to_bcd(uint8_t val) { return (uint8_t)(((val / 10) << 4) | (val % 10)); }
static uint8_t prv_bcd_to_bin(uint8_t val) { return (uint8_t)(((val >> 4) * 10) + (val & 0x0F)); }

static uint8_t prv_days_in_month(uint8_t month, uint16_t year) {
    if (month == 4 || month == 6 || month == 9 || month == 11) return 30;
    if (month == 2) {
        return ((((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0))) ? 29 : 28;
    }
    return 31;
}

static void prv_normalize_time(ds1307_time_t *time) {
    if (time->second > 59) time->second = 0;
    if (time->minute > 59) time->minute = 0;
    if (time->hour > 23) time->hour = 0;
    if (time->month < 1 || time->month > 12) time->month = 1;
    uint8_t max_d = prv_days_in_month(time->month, time->year);
    if (time->date < 1 || time->date > max_d) time->date = 1;
}

static void prv_sync_regs_from_time(ds1307_t *dev) {
    ds1307_time_t *t = &dev->current_time;
    uint8_t ch = dev->regs[DS1307_REG_SECONDS] & DS1307_BIT_CH;
    dev->regs[DS1307_REG_SECONDS] = prv_bin_to_bcd(t->second) | ch;
    dev->regs[DS1307_REG_MINUTES] = prv_bin_to_bcd(t->minute);
    dev->regs[DS1307_REG_HOURS]   = prv_bin_to_bcd(t->hour); /* Simple 24h for simulator */
    dev->regs[DS1307_REG_DAY]     = t->day_of_week & DS1307_MASK_DAY;
    dev->regs[DS1307_REG_DATE]    = prv_bin_to_bcd(t->date);
    dev->regs[DS1307_REG_MONTH]   = prv_bin_to_bcd(t->month);
    dev->regs[DS1307_REG_YEAR]    = prv_bin_to_bcd((uint8_t)(t->year % 100));
}

static void prv_sync_time_from_regs(ds1307_t *dev) {
    dev->current_time.second = prv_bcd_to_bin(dev->regs[DS1307_REG_SECONDS] & DS1307_MASK_SECONDS);
    dev->current_time.minute = prv_bcd_to_bin(dev->regs[DS1307_REG_MINUTES] & DS1307_MASK_MINUTES);
    dev->current_time.hour   = prv_bcd_to_bin(dev->regs[DS1307_REG_HOURS] & DS1307_MASK_HOURS_24);
    dev->current_time.day_of_week = dev->regs[DS1307_REG_DAY] & DS1307_MASK_DAY;
    dev->current_time.date   = prv_bcd_to_bin(dev->regs[DS1307_REG_DATE] & DS1307_MASK_DATE);
    dev->current_time.month  = prv_bcd_to_bin(dev->regs[DS1307_REG_MONTH] & DS1307_MASK_MONTH);
    dev->current_time.year   = 2000 + prv_bcd_to_bin(dev->regs[DS1307_REG_YEAR]);
}

static void prv_publish_regs(ds1307_t *dev) {
    /* Simple bank switching for simulator */
    uint8_t next_bank = (dev->active_bank + 1) % DS1307_BANK_COUNT;
    if (next_bank == dev->tx_bank && dev->tx_active) {
        next_bank = (next_bank + 1) % DS1307_BANK_COUNT;
    }
    memcpy(dev->reg_bank[next_bank], dev->regs, DS1307_REG_COUNT);
    dev->active_bank = next_bank;
}
