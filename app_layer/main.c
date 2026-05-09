#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"

#include "app_cfg.h"

#if (ENABLE_DS1307 == 1)
#include "ds1307_project.h"
#endif

#if (ENABLE_DHT11 == 1)
#include "dht11_project.h"
#endif

#if (ENABLE_MQ2 == 1)
#include "mq2_project.h"
#endif

/* ============================================================
 * Main Entry Point
 * ============================================================ */
int main(void)
{
    /* 1. System low-level initialization */
    SystemInit();
    SystemCoreClockUpdate();

    /* 2. Initialize Projects/Sensors */
#if (ENABLE_DS1307 == 1)
    ds1307_project_init();
#endif

#if (ENABLE_DHT11 == 1)
    dht11_project_init();
#endif

#if (ENABLE_MQ2 == 1)
    mq2_project_init();
#endif

    /* 3. Start FreeRTOS Scheduler */
    vTaskStartScheduler();

    /* Should never reach here */
    while (1);
}

/* ============================================================
 * FreeRTOS Hooks
 * ============================================================ */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask; (void)pcTaskName;
    while (1);
}

void vApplicationMallocFailedHook(void)
{
    while (1);
}
