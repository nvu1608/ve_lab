#ifndef SENSOR_DS1307_SIM_H
#define SENSOR_DS1307_SIM_H

#include "ds1307_api.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

/**
 * @brief DS1307 Constants
 */
#define DS1307_REG_COUNT        64u
#define DS1307_BANK_COUNT       3u

/**
 * @brief DS1307 Simulator Object Structure
 */
struct ds1307 {
    /* --- Core State --- */
    ds1307_time_t current_time;             /* Current time in binary format */
    uint8_t regs[DS1307_REG_COUNT];         /* Registers in BCD format (per datasheet) */

    /* --- Peripheral Interface --- */
    void *i2c_slave;                        /* Pointer to i2c_slave_t driver object */

    /* --- RTOS Objects --- */
    SemaphoreHandle_t mutex;                /* Protects register map and time data */
    SemaphoreHandle_t write_sem;            /* Notifies task when I2C write transaction is done */
    TaskHandle_t task_handle;               /* Handle for the simulator task */

    /* --- Simulation State --- */
    uint8_t current_reg;                    /* Current register pointer */
    uint8_t write_pending;                  /* Flag: new data received via I2C, waiting to apply */
    uint8_t write_buf[DS1307_REG_COUNT];    /* Staging buffer for I2C write transactions */
    uint8_t write_len;                      /* Length of data in write_buf */
    uint8_t write_start_reg;                /* Starting register of current write transaction */

    /* --- Triple Buffering (for stable I2C reads) --- */
    uint8_t reg_bank[DS1307_BANK_COUNT][DS1307_REG_COUNT];
    volatile uint8_t active_bank;           /* Most recent consistent register snapshot */
    volatile uint8_t tx_bank;               /* Bank currently being used by I2C TX */
    volatile uint8_t tx_active;              /* Flag: I2C read transaction in progress */
};

/**
 * @brief Initialize and start the DS1307 simulation task
 * 
 * @param dev Pointer to ds1307 object to initialize
 * @param i2c_slave Pointer to the I2C slave driver instance
 * @param task_priority Priority for the FreeRTOS task
 * @param stack_size Stack size for the FreeRTOS task
 */
void ds1307_sim_start(ds1307_t *dev, 
                      void *i2c_slave, 
                      uint32_t task_priority, 
                      uint32_t stack_size);

#endif /* SENSOR_DS1307_SIM_H */
