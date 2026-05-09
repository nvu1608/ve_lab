/**
 * @file    mq2_project.c
 * @brief   MQ-2 simulation project implementation.
 * @details Configures project-level hardware resources, initializes the DAC
 *          driver, and starts the MQ-2 simulator task.
 */

#include "mq2_project.h"

#include "app_cfg.h"
#include "mq2_project_cfg.h"

#include "driver_gpio.h"
#include "driver_dac.h"
#include "mq2_sim.h"

#if (ENABLE_MQ2 == 1)

/* Private objects ---------------------------------------------------------*/

static dac_t  g_mq2_dac;
static mq2_t  g_mq2_sim;
static gpio_t g_mq2_dac_pin;

/* Private function prototypes ---------------------------------------------*/

static void prv_setup_mq2_hw(void);
static void prv_setup_mq2_clock(void);
static void prv_setup_mq2_gpio(void);
static void prv_setup_mq2_dac(void);

/* Public functions --------------------------------------------------------*/

void mq2_project_init(void)
{
    prv_setup_mq2_hw();

    mq2_sim_start(&g_mq2_sim,
                  &g_mq2_dac,
                  MQ2_TASK_PRIO,
                  MQ2_TASK_STACK);
}

/* Private functions -------------------------------------------------------*/

/**
 * @brief Setup all hardware resources required by the MQ-2 simulator.
 */
static void prv_setup_mq2_hw(void)
{
    prv_setup_mq2_clock();
    prv_setup_mq2_gpio();
    prv_setup_mq2_dac();
}

/**
 * @brief Enable GPIO and DAC peripheral clocks.
 */
static void prv_setup_mq2_clock(void)
{
    RCC_APB2PeriphClockCmd(MQ2_RCC_GPIO, ENABLE);
    RCC_APB1PeriphClockCmd(MQ2_RCC_DAC, ENABLE);
}

/**
 * @brief Configure DAC output GPIO pin.
 * @details STM32F103 DAC output pin must be configured as analog input mode.
 */
static void prv_setup_mq2_gpio(void)
{
    (void)gpio_init(&g_mq2_dac_pin,
                    MQ2_GPIO_PORT,
                    MQ2_GPIO_DAC_PIN,
                    MQ2_GPIO_MODE,
                    MQ2_GPIO_SPEED);

    (void)gpio_start(&g_mq2_dac_pin);
}

/**
 * @brief Initialize and start DAC driver used by the MQ-2 simulator.
 */
static void prv_setup_mq2_dac(void)
{
    (void)dac_init(&g_mq2_dac,
                   MQ2_DAC_INSTANCE,
                   MQ2_DAC_CHANNEL,
                   MQ2_DAC_TRIGGER,
                   MQ2_DAC_OUTPUT_BUFFER,
                   MQ2_DAC_ALIGN,
                   MQ2_DAC_VREF_MV);

    (void)dac_start(&g_mq2_dac);
}

#endif /* ENABLE_MQ2 */