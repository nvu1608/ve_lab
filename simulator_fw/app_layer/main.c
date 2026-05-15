/**
 * @file    main.c
 * @brief   Application entry point for simulator firmware.
 * @details Initializes hardware, configures simulation projects based on
 *          app_cfg.h, and starts the FreeRTOS scheduler.
 */

#include "FreeRTOS.h"
#include "app_cfg.h"
#include "stm32f10x.h"
#include "task.h"

#if (ENABLE_DS1307 == 1)
#include "sim_ds1307.h"
#endif

#if (ENABLE_DHT11 == 1)
#include "sim_dht11.h"
#endif

#if (ENABLE_MQ2 == 1)
#include "sim_mq2.h"
#endif

#if (ENABLE_HC595 == 1)
#include "sim_74hc595.h"
#endif

#if (ENABLE_DHT22 == 1)
#include "sim_dht22.h"
#endif

/* Private function prototypes ---------------------------------------------*/

/**
 * @brief Initialize low-level MCU hardware (clocks, NVIC grouping).
 */
static void prv_setup_hardware(void);

/**
 * @brief Initialize all enabled simulation modules.
 */
static void prv_setup_labs(void);

/* Public functions --------------------------------------------------------*/

int main(void)
{
    /* 1. Low-level hardware setup */
    prv_setup_hardware();

    /* 2. Simulation modules setup */
    prv_setup_labs();

    /* 3. Start FreeRTOS scheduler */
    vTaskStartScheduler();

    /* Should never reach here */
    for (;;)
    {
        __NOP();
    }
}

/* Private functions -------------------------------------------------------*/

static void prv_setup_hardware(void)
{
    SystemInit();
    SystemCoreClockUpdate();

    /* Set NVIC priority grouping for FreeRTOS */
    NVIC_SetPriorityGrouping(NVIC_PriorityGroup_4);
}

static void prv_setup_labs(void)
{
#if (ENABLE_DS1307 == 1)
    sim_ds1307_init();
#endif

#if (ENABLE_DHT11 == 1)
    sim_dht11_init();
#endif

#if (ENABLE_MQ2 == 1)
    sim_mq2_init();
#endif

#if (ENABLE_HC595 == 1)
    sim_74hc595_init();
#endif

#if (ENABLE_DHT22 == 1)
    sim_dht22_init();
#endif
}


void vApplicationStackOverflowHook( TaskHandle_t xTask, char *pcTaskName )
{
    ( void ) xTask;
    ( void ) pcTaskName;
    for( ;; );
}

void vApplicationMallocFailedHook( void )
{
    for( ;; );
}
