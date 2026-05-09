/**
 * @file    ds1307_project.c
 * @brief   DS1307 simulation project implementation.
 * @details Configures project-level hardware resources, initializes the I2C
 *          slave driver, and starts the DS1307 simulator task.
 */

#include "ds1307_project.h"

#include "app_cfg.h"
#include "ds1307_project_cfg.h"

#include "driver_gpio.h"
#include "driver_i2c_slave.h"
#include "ds1307_sim.h"

#if (ENABLE_DS1307 == 1)

/* Private objects ---------------------------------------------------------*/

static i2c_slave_t g_ds1307_i2c;
static ds1307_t    g_ds1307_sim;

static gpio_t      g_ds1307_scl;
static gpio_t      g_ds1307_sda;

/* Private function prototypes ---------------------------------------------*/

/**
 * @brief Setup all hardware resources required by the DS1307 simulator.
 */
static void prv_setup_ds1307_hw(void);

/**
 * @brief Enable GPIO, AFIO, and I2C peripheral clocks.
 */
static void prv_setup_ds1307_clock(void);

/**
 * @brief Configure I2C GPIO pins.
 * @details DS1307 simulator uses I2C open-drain alternate-function pins.
 */
static void prv_setup_ds1307_gpio(void);

/**
 * @brief Configure NVIC for I2C event and error interrupts.
 */
static void prv_setup_ds1307_nvic(void);


static void prv_setup_ds1307_i2c(void);

/* Public functions --------------------------------------------------------*/

void ds1307_project_init(void)
{
    prv_setup_ds1307_hw();

    ds1307_sim_start(&g_ds1307_sim,
                     &g_ds1307_i2c,
                     DS1307_TASK_PRIO,
                     DS1307_TASK_STACK);
}

/* IRQ handlers ------------------------------------------------------------*/

void I2C1_EV_IRQHandler(void)
{
    i2c_slave_ev_irq_process(&g_ds1307_i2c);
}

void I2C1_ER_IRQHandler(void)
{
    i2c_slave_er_irq_process(&g_ds1307_i2c);
}

/* Private functions -------------------------------------------------------*/

static void prv_setup_ds1307_hw(void)
{
    prv_setup_ds1307_clock();
    prv_setup_ds1307_gpio();
    prv_setup_ds1307_nvic();
    prv_setup_ds1307_i2c();
}

static void prv_setup_ds1307_clock(void)
{
    RCC_APB2PeriphClockCmd(DS1307_RCC_GPIO | DS1307_RCC_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(DS1307_RCC_I2C, ENABLE);
}

static void prv_setup_ds1307_gpio(void)
{
    (void)gpio_init(&g_ds1307_scl,
                    DS1307_GPIO_PORT,
                    DS1307_GPIO_SCL_PIN,
                    DS1307_GPIO_MODE,
                    DS1307_GPIO_SPEED);

    (void)gpio_start(&g_ds1307_scl);

    (void)gpio_init(&g_ds1307_sda,
                    DS1307_GPIO_PORT,
                    DS1307_GPIO_SDA_PIN,
                    DS1307_GPIO_MODE,
                    DS1307_GPIO_SPEED);

    (void)gpio_start(&g_ds1307_sda);
}

static void prv_setup_ds1307_nvic(void)
{
    NVIC_InitTypeDef nvic_init;

    NVIC_PriorityGroupConfig(DS1307_NVIC_PRIORITY_GROUP);

    nvic_init.NVIC_IRQChannel = DS1307_IRQ_EV;
    nvic_init.NVIC_IRQChannelPreemptionPriority = DS1307_IRQ_PREEMPT_PRIO;
    nvic_init.NVIC_IRQChannelSubPriority = DS1307_IRQ_SUB_PRIO;
    nvic_init.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic_init);

    nvic_init.NVIC_IRQChannel = DS1307_IRQ_ER;
    nvic_init.NVIC_IRQChannelPreemptionPriority = DS1307_IRQ_PREEMPT_PRIO;
    nvic_init.NVIC_IRQChannelSubPriority = DS1307_IRQ_SUB_PRIO;
    nvic_init.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic_init);
}

static void prv_setup_ds1307_i2c(void)
{
    (void)i2c_slave_init(&g_ds1307_i2c,
                         DS1307_I2C_INSTANCE,
                         DS1307_I2C_ADDR,
                         DS1307_I2C_SPEED,
                         DS1307_I2C_DUTY);
}

#endif /* ENABLE_DS1307 */
