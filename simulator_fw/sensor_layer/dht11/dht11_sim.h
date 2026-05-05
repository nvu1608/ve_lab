#ifndef DHT11_SIM_H
#define DHT11_SIM_H

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "dht11_api.h"
#include <stdint.h>

/**
 * @brief DHT11 Simulator Object Structure
 */
struct dht11
{
    /* Drivers */
    void *gpio_pin;   /* gpio_t pointer */
    void *tim_tx;     /* timer_t pointer */
    void *tim_rx;     /* timer_t pointer */

    /* Data Frame (40 bits) */
    uint8_t data[5];

    /* RTOS Primitives */
    TaskHandle_t      task_handle;
    SemaphoreHandle_t start_sem;
    QueueHandle_t     data_queue;
    
    /* Internal State */
    volatile uint8_t  tx_state;
    volatile uint8_t  bit_idx;
    volatile uint8_t  ic_edge;
    volatile uint16_t ic_t_fall;
};

/**
 * @brief Initialize and start DHT11 simulation task
 * 
 * @param dev           DHT11 object
 * @param gpio_pin      GPIO object (gpio_t)
 * @param tim_tx        Timer object for Output Compare (timer_t)
 * @param tim_rx        Timer object for Input Capture (timer_t)
 * @param task_priority Priority of the simulation task
 * @param stack_size    Stack size of the simulation task
 */
void dht11_sim_start(dht11_t *dev,
                     void *gpio_pin,
                     void *tim_tx,
                     void *tim_rx,
                     uint32_t task_priority,
                     uint32_t stack_size);

#endif /* DHT11_SIM_H */
