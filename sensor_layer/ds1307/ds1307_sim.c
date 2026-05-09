/**
 * @file    ds1307_sim.c
 * @brief   DS1307 sensor simulation implementation.
 * @details Implements DS1307 register map behavior, time ticking,
 *          I2C slave transaction handling, and public simulation APIs.
 */

#include <string.h>

#include "ds1307_sim.h"
#include "driver_i2c_slave.h"

/* Private macros ----------------------------------------------------------*/

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


/* Private function prototypes ---------------------------------------------*/

/**
 * @brief Convert binary value to BCD format.
 */
static uint8_t prv_bin_to_bcd(uint8_t val);

/**
 * @brief Convert BCD value to binary format.
 */
static uint8_t prv_bcd_to_bin(uint8_t val);

/**
 * @brief Get number of days in a month.
 * @details Handles leap year calculation for February.
 */
static uint8_t prv_days_in_month(uint8_t month, uint16_t year);

/**
 * @brief Normalize invalid time fields into valid DS1307 ranges.
 * @details Out-of-range fields are reset to safe default values.
 */
static void prv_normalize_time(ds1307_time_t *time);

/**
 * @brief Increment binary time by one second.
 * @details Handles minute, hour, day, month, year rollover and day-of-week
 *          update.
 */
static void prv_increment_time(ds1307_time_t *time);

/**
 * @brief Update binary time object from DS1307 register map.
 * @details Converts BCD register values to binary time fields.
 */
static void prv_sync_time_from_regs(ds1307_t *dev);

/**
 * @brief Update DS1307 register map from binary time object.
 * @details Converts binary time fields to BCD register values while preserving
 *          the CH oscillator halt bit.
 */
static void prv_sync_regs_from_time(ds1307_t *dev);

/**
 * @brief Publish register map to read snapshot bank.
 * @details Uses triple buffering so I2C read transactions do not observe a
 *          partially updated register map.
 */
static void prv_publish_regs(ds1307_t *dev);


/**
 * @brief Handle events emitted by the I2C slave driver.
 * @details Tracks DS1307 write transactions, stores received bytes into a
 *          staging buffer, and freezes a register bank for read transactions.
 */
static void prv_i2c_event_handler(void *ctx, const i2c_slave_evt_t *evt);

/**
 * @brief Provide one byte for an I2C master read transaction.
 * @details Reads from the frozen TX bank and auto-increments the DS1307
 *          register pointer.
 */
static uint8_t prv_i2c_tx_handler(void *ctx);

/**
 * @brief Main DS1307 simulator task.
 * @details Applies completed I2C writes and increments simulated RTC time
 *          every second when the CH bit is cleared.
 */
static void prv_sim_task(void *argument);

/* Public functions --------------------------------------------------------*/

void ds1307_set_time(ds1307_t *dev, const ds1307_time_t *time)
{
    if ((dev == NULL) || (time == NULL))
    {
        return;
    }

    if (xSemaphoreTake(dev->mutex, portMAX_DELAY) == pdTRUE)
    {
        dev->current_time = *time;

        prv_normalize_time(&dev->current_time);
        prv_sync_regs_from_time(dev);
        prv_publish_regs(dev);

        xSemaphoreGive(dev->mutex);
    }
}

void ds1307_get_time(ds1307_t *dev, ds1307_time_t *time)
{
    if ((dev == NULL) || (time == NULL))
    {
        return;
    }

    if (xSemaphoreTake(dev->mutex, portMAX_DELAY) == pdTRUE)
    {
        *time = dev->current_time;

        xSemaphoreGive(dev->mutex);
    }
}

uint8_t ds1307_is_halted(const ds1307_t *dev)
{
    if (dev == NULL)
    {
        return 1u;
    }

    return ((dev->regs[DS1307_REG_SECONDS] & DS1307_BIT_CH) != 0u) ? 1u : 0u;
}

void ds1307_sim_start(ds1307_t *dev,
                      void *i2c_slave,
                      uint32_t task_priority,
                      uint32_t stack_size) 
{
    if (dev == NULL || i2c_slave == NULL)
    {
        return;
    }

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
    i2c_slave_set_event_callback((i2c_slave_t *)dev->i2c_slave,
                                 prv_i2c_event_handler,
                                 dev);

    i2c_slave_set_tx_byte_callback((i2c_slave_t *)dev->i2c_slave,
                                   prv_i2c_tx_handler,
                                   dev);

    /* Create Simulation Task */
    xTaskCreate(prv_sim_task,
                "DS1307_Sim",
                (uint16_t)stack_size,
                dev,
                task_priority,
                &dev->task_handle);

    /* Start I2C Driver */
    i2c_slave_start((i2c_slave_t *)dev->i2c_slave);
}

/* Private functions -------------------------------------------------------*/

static void prv_sim_task(void *argument)
{
    ds1307_t *dev = (ds1307_t *)argument;

    while (1)
    {
        /*
         * The simulator task has two jobs:
         * 1. Apply completed I2C write transactions.
         * 2. Tick the RTC time every 1000 ms when no write occurs.
         */
        if (xSemaphoreTake(dev->write_sem, pdMS_TO_TICKS(1000)) == pdTRUE)
        {
            if (xSemaphoreTake(dev->mutex, portMAX_DELAY) == pdTRUE)
            {
                uint8_t reg = dev->write_start_reg;

                /*
                 * Apply staged bytes into the DS1307 register map.
                 * The register pointer wraps around like a simple circular map.
                 */
                for (uint8_t i = 0u; i < dev->write_len; i++)
                {
                    dev->regs[reg] = dev->write_buf[i];
                    reg = (uint8_t)((reg + 1u) % DS1307_REG_COUNT);
                }

                /*
                 * If time registers were touched, update the binary time object.
                 */
                if (dev->write_start_reg <= DS1307_REG_YEAR)
                {
                    prv_sync_time_from_regs(dev);
                }

                dev->write_pending = 0u;
                dev->write_len = 0u;

                prv_publish_regs(dev);

                xSemaphoreGive(dev->mutex);
            }
        }
        else
        {
            if (xSemaphoreTake(dev->mutex, portMAX_DELAY) == pdTRUE)
            {
                /*
                 * CH bit in seconds register halts the oscillator.
                 * If halted, simulated time does not advance.
                 */
                if (ds1307_is_halted(dev) == 0u)
                {
                    prv_increment_time(&dev->current_time);
                    prv_sync_regs_from_time(dev);
                    prv_publish_regs(dev);
                }

                xSemaphoreGive(dev->mutex);
            }
        }
    }
}

static void prv_i2c_event_handler(void *ctx, const i2c_slave_evt_t *evt)
{
    ds1307_t *dev = (ds1307_t *)ctx;
    BaseType_t woken = pdFALSE;

    if ((dev == NULL) || (evt == NULL))
    {
        return;
    }

    switch (evt->type)
    {
        case I2C_SLAVE_EVT_WRITE_START:
            dev->write_len = 0u;
            dev->write_pending = 0u;
            break;

        case I2C_SLAVE_EVT_BYTE_RECEIVED:
            /*
             * DS1307 write format:
             * First byte is register address pointer.
             * Following bytes are data written to consecutive registers.
             */
            if ((dev->write_pending == 0u) && (dev->write_len == 0u))
            {
                dev->current_reg = (uint8_t)(evt->data % DS1307_REG_COUNT);
                dev->write_start_reg = dev->current_reg;
                dev->write_pending = 1u;
            }
            else
            {
                if (dev->write_len < DS1307_REG_COUNT)
                {
                    dev->write_buf[dev->write_len] = evt->data;
                    dev->write_len++;
                }
            }
            break;

        case I2C_SLAVE_EVT_WRITE_DONE:
            if (dev->write_len > 0u)
            {
                xSemaphoreGiveFromISR(dev->write_sem, &woken);
            }
            break;

        case I2C_SLAVE_EVT_READ_START:
            /*
             * Freeze the register snapshot used for this I2C read transaction.
             */
            dev->tx_bank = dev->active_bank;
            dev->tx_active = 1u;
            break;

        case I2C_SLAVE_EVT_READ_DONE:
            dev->tx_active = 0u;
            break;

        default:
            break;
    }

    portYIELD_FROM_ISR(woken);
}

static uint8_t prv_i2c_tx_handler(void *ctx)
{
    ds1307_t *dev = (ds1307_t *)ctx;
    uint8_t data;

    if (dev == NULL)
    {
        return 0xFFu;
    }

    data = dev->reg_bank[dev->tx_bank][dev->current_reg];

    dev->current_reg = (uint8_t)((dev->current_reg + 1u) %
                                 DS1307_REG_COUNT);

    return data;
}

static uint8_t prv_bin_to_bcd(uint8_t val)
{
    return (uint8_t)(((val / 10u) << 4u) | (val % 10u));
}

static uint8_t prv_bcd_to_bin(uint8_t val)
{
    return (uint8_t)(((val >> 4u) * 10u) + (val & 0x0Fu));
}

static uint8_t prv_days_in_month(uint8_t month, uint16_t year)
{
    if ((month == 4u) ||
        (month == 6u) ||
        (month == 9u) ||
        (month == 11u))
    {
        return 30u;
    }

    if (month == 2u)
    {
        if (((year % 4u) == 0u && (year % 100u) != 0u) ||
            ((year % 400u) == 0u))
        {
            return 29u;
        }

        return 28u;
    }

    return 31u;
}

static void prv_normalize_time(ds1307_time_t *time)
{
    uint8_t max_day;

    if (time == NULL)
    {
        return;
    }

    if (time->second > 59u)
    {
        time->second = 0u;
    }

    if (time->minute > 59u)
    {
        time->minute = 0u;
    }

    if (time->hour > 23u)
    {
        time->hour = 0u;
    }

    if ((time->day_of_week < 1u) || (time->day_of_week > 7u))
    {
        time->day_of_week = 1u;
    }

    if ((time->month < 1u) || (time->month > 12u))
    {
        time->month = 1u;
    }

    max_day = prv_days_in_month(time->month, time->year);

    if ((time->date < 1u) || (time->date > max_day))
    {
        time->date = 1u;
    }
}

static void prv_increment_time(ds1307_time_t *time)
{
    if (time == NULL)
    {
        return;
    }

    time->second++;

    if (time->second >= 60u)
    {
        time->second = 0u;
        time->minute++;

        if (time->minute >= 60u)
        {
            time->minute = 0u;
            time->hour++;

            if (time->hour >= 24u)
            {
                time->hour = 0u;
                time->day_of_week++;

                if (time->day_of_week > 7u)
                {
                    time->day_of_week = 1u;
                }

                time->date++;

                if (time->date >
                    prv_days_in_month(time->month, time->year))
                {
                    time->date = 1u;
                    time->month++;

                    if (time->month > 12u)
                    {
                        time->month = 1u;
                        time->year++;
                    }
                }
            }
        }
    }
}

static void prv_sync_regs_from_time(ds1307_t *dev)
{
    ds1307_time_t *t;
    uint8_t hour_reg;
    uint8_t is_12h_mode;
    uint8_t hour_12;
    uint8_t pm;

    if (dev == NULL)
    {
        return;
    }

    t = &dev->current_time;

    dev->regs[DS1307_REG_SECONDS] =
        prv_bin_to_bcd(t->second) |
        (dev->regs[DS1307_REG_SECONDS] & DS1307_BIT_CH);

    dev->regs[DS1307_REG_MINUTES] = prv_bin_to_bcd(t->minute);

    hour_reg = dev->regs[DS1307_REG_HOURS];
    is_12h_mode = hour_reg & DS1307_BIT_12H;

    if (is_12h_mode != 0u)
    {
        pm = (t->hour >= 12u) ? DS1307_BIT_PM : 0u;

        hour_12 = (uint8_t)(t->hour % 12u);
        if (hour_12 == 0u)
        {
            hour_12 = 12u;
        }

        dev->regs[DS1307_REG_HOURS] =
            DS1307_BIT_12H | pm | prv_bin_to_bcd(hour_12);
    }
    else
    {
        dev->regs[DS1307_REG_HOURS] = prv_bin_to_bcd(t->hour);
    }

    dev->regs[DS1307_REG_DAY] = t->day_of_week & DS1307_MASK_DAY;
    dev->regs[DS1307_REG_DATE] = prv_bin_to_bcd(t->date);
    dev->regs[DS1307_REG_MONTH] = prv_bin_to_bcd(t->month);
    dev->regs[DS1307_REG_YEAR] =
        prv_bin_to_bcd((uint8_t)(t->year % 100u));
}

static void prv_sync_time_from_regs(ds1307_t *dev)
{
    uint8_t hour_reg;
    uint8_t hour;

    if (dev == NULL)
    {
        return;
    }

    dev->current_time.second =
        prv_bcd_to_bin(dev->regs[DS1307_REG_SECONDS] &
                       DS1307_MASK_SECONDS);

    dev->current_time.minute =
        prv_bcd_to_bin(dev->regs[DS1307_REG_MINUTES] &
                       DS1307_MASK_MINUTES);

    hour_reg = dev->regs[DS1307_REG_HOURS];

    if ((hour_reg & DS1307_BIT_12H) != 0u)
    {
        hour = prv_bcd_to_bin(hour_reg & DS1307_MASK_HOURS_12);

        if (hour == 12u)
        {
            hour = 0u;
        }

        if ((hour_reg & DS1307_BIT_PM) != 0u)
        {
            hour += 12u;
        }

        dev->current_time.hour = hour;
    }
    else
    {
        dev->current_time.hour =
            prv_bcd_to_bin(hour_reg & DS1307_MASK_HOURS_24);
    }

    dev->current_time.day_of_week =
        dev->regs[DS1307_REG_DAY] & DS1307_MASK_DAY;

    dev->current_time.date =
        prv_bcd_to_bin(dev->regs[DS1307_REG_DATE] &
                       DS1307_MASK_DATE);

    dev->current_time.month =
        prv_bcd_to_bin(dev->regs[DS1307_REG_MONTH] &
                       DS1307_MASK_MONTH);

    dev->current_time.year =
        2000u + prv_bcd_to_bin(dev->regs[DS1307_REG_YEAR]);

    prv_normalize_time(&dev->current_time);
}

static void prv_publish_regs(ds1307_t *dev)
{
    uint8_t next_bank;

    if (dev == NULL)
    {
        return;
    }

    /*
     * Publish a new register snapshot for I2C reads.
     * If the next bank is currently used by TX, skip to another bank.
     */
    next_bank = (uint8_t)((dev->active_bank + 1u) % DS1307_BANK_COUNT);

    if ((next_bank == dev->tx_bank) && (dev->tx_active != 0u))
    {
        next_bank = (uint8_t)((next_bank + 1u) % DS1307_BANK_COUNT);
    }

    memcpy(dev->reg_bank[next_bank], dev->regs, DS1307_REG_COUNT);

    dev->active_bank = next_bank;
}
