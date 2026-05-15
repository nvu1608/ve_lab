/**
 * @file    sim_dht11.h
 * @brief   DHT11 Simulation Module (Self-contained).
 */

#ifndef SIM_DHT11_H
#define SIM_DHT11_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ----------------------------------------------------------------*/
#include "dht11.h"
#include "driver_tim.h"
#include "driver_gpio.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

/* Exported types ----------------------------------------------------------*/

/**
 * @brief DHT11 Simulation object.
 */
typedef struct {
    dht11_t           logic;       /**< Pure logic instance */
    timer_t           ic;          /**< Timer for measuring host start pulse */
    timer_t           oc;          /**< Timer for precise response bit timing */
    gpio_t            pin;         /**< Single-wire data pin */
    
    SemaphoreHandle_t start_sem;   /**< Signal for host start pulse detection */
    TaskHandle_t      task_handle; /**< Handle for simulation task */
} sim_dht11_t;

/* Public API --------------------------------------------------------------*/

/**
 * @brief Initialize and start DHT11 simulation.
 * @note  Self-contained module.
 */
void sim_dht11_init(void);

#ifdef __cplusplus
}
#endif

#endif /* SIM_DHT11_H */
