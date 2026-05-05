#include "ds1307_project.h"
#include "ds1307_project_cfg.h"
#include "app_cfg.h"
#include "driver_rcc.h"
#include "driver_gpio.h"
#include "driver_nvic.h"
#include "driver_i2c_slave.h"
#include "ds1307_sim.h"

#if (ENABLE_DS1307 == 1)

/* ============================================================
 * Project Private Objects
 * ============================================================ */
static i2c_slave_t g_ds1307_i2c;
static ds1307_t    g_ds1307_sim;

static gpio_t      g_ds1307_scl;
static gpio_t      g_ds1307_sda;

/* ============================================================
 * Private Function Prototypes
 * ============================================================ */
static void prv_setup_ds1307_hw(void);

/* ============================================================
 * Public API
 * ============================================================ */

void ds1307_project_init(void)
{
    /* 1. Setup Hardware (RCC, GPIO, NVIC, Driver) */
    prv_setup_ds1307_hw();

    /* 2. Start Sensor Simulator Task */
    ds1307_sim_start(&g_ds1307_sim, 
                     &g_ds1307_i2c, 
                     DS1307_TASK_PRIO, 
                     DS1307_TASK_STACK);
}

/* ============================================================
 * Private Hardware Setup
 * ============================================================ */

static void prv_setup_ds1307_hw(void)
{
    /* 1. Enable Clocks */
    rcc_enable(RCC_BUS_APB2, DS1307_RCC_GPIO | DS1307_RCC_AFIO);
    rcc_enable(RCC_BUS_APB1, DS1307_RCC_I2C);

    /* 2. Configure GPIO Pins (I2C1: PB6-SCL, PB7-SDA) */
    gpio_init(&g_ds1307_scl, DS1307_GPIO_PORT, DS1307_GPIO_SCL_PIN, GPIO_Mode_AF_OD, GPIO_Speed_50MHz);
    gpio_start(&g_ds1307_scl);

    gpio_init(&g_ds1307_sda, DS1307_GPIO_PORT, DS1307_GPIO_SDA_PIN, GPIO_Mode_AF_OD, GPIO_Speed_50MHz);
    gpio_start(&g_ds1307_sda);

    /* 3. Configure NVIC for I2C Interrupts */
    nvic_enable_irq(DS1307_IRQ_EV, DS1307_IRQ_PRIO, 0);
    nvic_enable_irq(DS1307_IRQ_ER, DS1307_IRQ_PRIO, 0);

    /* 4. Initialize I2C Slave Driver */
    i2c_slave_init(&g_ds1307_i2c, 
                   DS1307_I2C_INSTANCE, 
                   DS1307_I2C_ADDR, 
                   DS1307_I2C_SPEED, 
                   DS1307_I2C_DUTY);
}

/* 
 * I2C Interrupt Handlers
 * Bridging hardware vectors to project objects.
 */
void I2C1_EV_IRQHandler(void)
{
    i2c_slave_ev_irq_handler(&g_ds1307_i2c);
}

void I2C1_ER_IRQHandler(void)
{
    i2c_slave_er_irq_handler(&g_ds1307_i2c);
}

#endif /* ENABLE_DS1307 */
