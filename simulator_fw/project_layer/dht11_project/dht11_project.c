#include "dht11_project.h"
#include "dht11_project_cfg.h"
#include "app_cfg.h"
#include "driver_rcc.h"
#include "driver_gpio.h"
#include "driver_nvic.h"
#include "driver_timer.h"
#include "dht11_sim.h"

#if (ENABLE_DHT11 == 1)

/* ============================================================
 * Project Private Objects
 * ============================================================ */
static dht11_t g_dht11_sim;
static gpio_t  g_dht11_pin;
static timer_t g_dht11_tim_tx;
static timer_t g_dht11_tim_rx;

/* ============================================================
 * Private Function Prototypes
 * ============================================================ */
static void prv_setup_dht11_hw(void);

/* ============================================================
 * Public API
 * ============================================================ */

void dht11_project_init(void)
{
    /* 1. Setup Hardware */
    prv_setup_dht11_hw();

    /* 2. Start Sensor Simulator Task */
    dht11_sim_start(&g_dht11_sim,
                    &g_dht11_pin,
                    &g_dht11_tim_tx,
                    &g_dht11_tim_rx,
                    DHT11_TASK_PRIO,
                    DHT11_TASK_STACK);
}

/* ============================================================
 * Private Hardware Setup
 * ============================================================ */

static void prv_setup_dht11_hw(void)
{
    /* 1. Enable Clocks */
    rcc_enable(RCC_BUS_APB2, DHT11_RCC_GPIO | DHT11_RCC_AFIO | DHT11_RCC_TIM_TX);
    rcc_enable(RCC_BUS_APB1, DHT11_RCC_TIM_RX);

    /* 2. Configure GPIO (Initial mode: IPU for listening) */
    gpio_init(&g_dht11_pin, DHT11_GPIO_PORT, DHT11_GPIO_PIN, GPIO_Mode_IPU, GPIO_Speed_50MHz);
    gpio_start(&g_dht11_pin);

    /* 3. Initialize Timers */
    timer_init(&g_dht11_tim_tx, DHT11_TIM_TX_INSTANCE);
    timer_init(&g_dht11_tim_rx, DHT11_TIM_RX_INSTANCE);

    /* 4. Configure NVIC */
    nvic_enable_irq(DHT11_IRQ_TIM_TX, DHT11_IRQ_PRIO, 0);
    nvic_enable_irq(DHT11_IRQ_TIM_RX, DHT11_IRQ_PRIO, 1);
}

/* 
 * Timer Interrupt Handlers
 */
void TIM1_CC_IRQHandler(void)
{
    timer_irq_handler(&g_dht11_tim_tx);
}

void TIM2_IRQHandler(void)
{
    timer_irq_handler(&g_dht11_tim_rx);
}

#endif /* ENABLE_DHT11 */
