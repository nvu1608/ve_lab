/**
 * @file    sim_ds1307.c
 * @brief   Self-contained DS1307 Simulation Module implementation.
 */

#include "sim_ds1307.h"
#include <string.h>

/* Private macros ----------------------------------------------------------*/
#define SIM_DS1307_I2C_INST     I2C1
#define SIM_DS1307_ADDR         0x68
#define SIM_DS1307_I2C_SPEED    100000
#define SIM_DS1307_STACK_SIZE   256
#define SIM_DS1307_TASK_PRIO    (tskIDLE_PRIORITY + 1)

/* Private function prototypes ---------------------------------------------*/
static void    prv_i2c_event_cb(void *ctx, const i2c_slave_evt_t *evt);
static uint8_t prv_i2c_tx_cb(void *ctx);
static void    prv_sim_task(void *argument);

/* Static objects ----------------------------------------------------------*/
static sim_ds1307_t g_ds1307_sim;

/* Public functions --------------------------------------------------------*/

/**
 * @brief Initialize and start the DS1307 simulation module.
 */
void sim_ds1307_init(void)
{
    memset(&g_ds1307_sim, 0, sizeof(sim_ds1307_t));

    /* 1. Logic layer initialization */
    ds1307_reset(&g_ds1307_sim.logic);

    /* 2. RTOS synchronization objects */
    g_ds1307_sim.mutex = xSemaphoreCreateMutex();

    /* 3. Driver layer setup and registration */
    i2c_slave_setup(&g_ds1307_sim.i2c, 
                    SIM_DS1307_I2C_INST, 
                    SIM_DS1307_ADDR, 
                    SIM_DS1307_I2C_SPEED, 
                    0);
                    
    i2c_slave_register_event(&g_ds1307_sim.i2c, prv_i2c_event_cb, &g_ds1307_sim);
    i2c_slave_register_tx(&g_ds1307_sim.i2c, prv_i2c_tx_cb, &g_ds1307_sim);
    
    /* Enable hardware communication */
    i2c_slave_enable(&g_ds1307_sim.i2c);

    /* 4. Simulation task creation */
    xTaskCreate(prv_sim_task, 
                "Sim_DS1307", 
                SIM_DS1307_STACK_SIZE, 
                &g_ds1307_sim, 
                SIM_DS1307_TASK_PRIO, 
                &g_ds1307_sim.task_handle);
}

/* IRQ Handlers ------------------------------------------------------------*/

/**
 * @brief I2C1 Event Interrupt Handler.
 * @note This handler is encapsulated within the simulation module.
 */
void I2C1_EV_IRQHandler(void)
{
    i2c_slave_handle_ev(&g_ds1307_sim.i2c);
}

/**
 * @brief I2C1 Error Interrupt Handler.
 */
void I2C1_ER_IRQHandler(void)
{
    i2c_slave_handle_er(&g_ds1307_sim.i2c);
}

/* Private functions -------------------------------------------------------*/

/**
 * @brief Callback to handle I2C slave events.
 */
static void prv_i2c_event_cb(void *ctx, const i2c_slave_evt_t *evt)
{
    sim_ds1307_t *sim = (sim_ds1307_t *)ctx;
    
    switch (evt->type)
    {
        case I2C_SLAVE_EVT_WRITE_START:
            /* First byte received will be register address */
            sim->logic.current_reg_ptr = 0xFF; // Mark as waiting for address
            break;

        case I2C_SLAVE_EVT_BYTE_RECEIVED:
            if (sim->logic.current_reg_ptr == 0xFF)
            {
                /* This is the register address */
                ds1307_rx(&sim->logic, evt->data, 1);
            }
            else
            {
                /* This is data to write */
                ds1307_rx(&sim->logic, evt->data, 0);
            }
            break;

        default:
            break;
    }
}

/**
 * @brief Callback to provide data for I2C transmit buffer.
 */
static uint8_t prv_i2c_tx_cb(void *ctx)
{
    sim_ds1307_t *sim = (sim_ds1307_t *)ctx;
    
    /* Delegate to logic layer */
    return ds1307_tx(&sim->logic);
}

/**
 * @brief Simulation background task.
 */
static void prv_sim_task(void *argument)
{
    sim_ds1307_t *sim = (sim_ds1307_t *)argument;
    TickType_t last_wake_time = xTaskGetTickCount();
    
    for (;;)
    {
        /* Advance logic time every 1 second */
        if (xTaskGetTickCount() - last_wake_time >= configTICK_RATE_HZ)
        {
            if (xSemaphoreTake(sim->mutex, portMAX_DELAY) == pdTRUE)
            {
                ds1307_tick(&sim->logic);
                xSemaphoreGive(sim->mutex);
            }
            last_wake_time += configTICK_RATE_HZ;
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
