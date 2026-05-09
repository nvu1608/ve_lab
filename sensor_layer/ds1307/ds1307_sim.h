/**
 * @file    ds1307_sim.h
 * @brief   DS1307 sensor simulation object and control API.
 * @details Defines the DS1307 simulator object, register storage, RTOS objects,
 *          I2C transaction state, and API to start the simulation task.
 */

#ifndef DS1307_SIM_H
#define DS1307_SIM_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Includes ----------------------------------------------------------------*/

#include <stdint.h>

#include "ds1307_api.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

/* Exported macros ---------------------------------------------------------*/

/**
 * @brief DS1307 register count.
 */
#define DS1307_REG_COUNT        64u

/**
 * @brief Register bank count for stable I2C read snapshots.
 */
#define DS1307_BANK_COUNT       3u


/* Exported types ----------------------------------------------------------*/

/**
 * @brief DS1307 simulator object.
 */
struct ds1307
{
    /* Core state ----------------------------------------------------------*/

    ds1307_time_t current_time; /**< Current time in binary format */
    uint8_t       regs[DS1307_REG_COUNT]; /**< Register map in BCD format */

    /* Peripheral interface -----------------------------------------------*/

    void *i2c_slave; /**< Pointer to I2C slave driver object */

    /* RTOS objects --------------------------------------------------------*/

    SemaphoreHandle_t mutex;       /**< Protects register map and time data */
    SemaphoreHandle_t write_sem;   /**< Notifies task when I2C write is done */
    TaskHandle_t      task_handle; /**< Simulator task handle */

    /* I2C write transaction state ----------------------------------------*/

    uint8_t current_reg;     /**< Current DS1307 register pointer */
    uint8_t write_pending;   /**< First-byte address phase flag */
    uint8_t write_buf[DS1307_REG_COUNT]; /**< Staging buffer for I2C writes */
    uint8_t write_len;       /**< Number of staged write bytes */
    uint8_t write_start_reg; /**< First register of current write transaction */

    /* Triple buffering for stable I2C reads ------------------------------*/

    uint8_t reg_bank[DS1307_BANK_COUNT][DS1307_REG_COUNT];

    volatile uint8_t active_bank; /**< Latest consistent register snapshot */
    volatile uint8_t tx_bank;     /**< Snapshot bank used by current I2C TX */
    volatile uint8_t tx_active;   /**< I2C read transaction active flag */
};

/* Public API --------------------------------------------------------------*/

/**
 * @brief Initialize and start DS1307 simulation task.
 *
 * @param dev Pointer to DS1307 simulator object.
 * @param i2c_slave Pointer to I2C slave driver object.
 * @param task_priority FreeRTOS task priority.
 * @param stack_size FreeRTOS task stack size.
 */
void ds1307_sim_start(ds1307_t *dev,
                      void *i2c_slave,
                      uint32_t task_priority,
                      uint32_t stack_size);

#ifdef __cplusplus
}
#endif

#endif /* DS1307_SIM_H */
