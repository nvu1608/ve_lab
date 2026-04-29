#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"

#include "app_config.h"

#if (ENABLE_DS1307 == 1)
#include "ds1307.h"
#endif

#if (ENABLE_DHT11 == 1)
#include "dht11.h"
#endif

/* ============================================================
 * ENTRY POINT
 * ============================================================ */
int main(void)
{
    /* 1. Cau hinh he thong co ban */
    SystemInit();
    SystemCoreClockUpdate();
    
    /* Dam bao NVIC Priority Group 4 cho FreeRTOS */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

    /* 2. Khoi tao cac Sensor duoc enable */
#if (ENABLE_DS1307 == 1)
    ds1307_sim_init();
#endif

#if (ENABLE_DHT11 == 1)
    dht11_sim_init();
#endif

    /* 3. Chay RTOS Scheduler */
    vTaskStartScheduler();

    /* 4. Fallback neu thieu RAM tao Idle task */
    while (1)
    {
    }
}

/* ============================================================
 * FreeRTOS Hooks
 * ============================================================ */

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    while (1);
}

void vApplicationMallocFailedHook(void)
{
    while (1);
}

void vApplicationIdleHook(void)
{
}

void vApplicationTickHook(void)
{
}
