/**
 * @file    sim_dht22.c
 * @brief   Self-contained DHT22 Simulation Module.
 */

#include "sim_dht22.h"
#include <string.h>

/* Private macros ----------------------------------------------------------*/

#define GPIO_PORT       GPIOA
#define GPIO_PIN        GPIO_Pin_1
#define TIM_IC_INST     TIM4
#define TIM_OC_INST     TIM5

#define STACK_SIZE      256
#define TASK_PRIO       (tskIDLE_PRIORITY + 2)

/* Protocol timing constants (microseconds) */
#define DHT_HOST_MIN_LOW_US 1000u
#define DHT_RESP_LOW_US     84u
#define DHT_RESP_HIGH_US    84u
#define DHT_BIT_LOW_US      54u
#define DHT_BIT0_HIGH_US    30u
#define DHT_BIT1_HIGH_US    74u
#define DHT_END_LOW_US      54u

/* Private types -----------------------------------------------------------*/

typedef enum {
    RESP_IDLE = 0,
    RESP_START_LOW,
    RESP_START_HIGH,
    RESP_BIT_LOW,
    RESP_BIT_HIGH,
    RESP_STOP_LOW,
    RESP_DONE
} dht22_state_t;

/* Static objects ----------------------------------------------------------*/

static sim_dht22_t g_dht22_sim;
static volatile dht22_state_t g_state;
static uint8_t g_ic_awaiting_rise;
static uint32_t g_ic_fall_capture;

/* Private function prototypes ---------------------------------------------*/

static void prv_task(void *arg);
static void prv_oc_cb(void *ctx, const timer_evt_t *evt);
static void prv_ic_cb(void *ctx, const timer_evt_t *evt);
static void prv_set_pin_output(void);
static void prv_set_pin_input(void);
static void prv_schedule_oc(uint16_t us);

/* Public functions --------------------------------------------------------*/

void sim_dht22_init(void)
{
    memset(&g_dht22_sim, 0, sizeof(sim_dht22_t));

    /* 1. Logic reset */
    dht22_reset(&g_dht22_sim.logic);

    /* 2. RTOS objects */
    g_dht22_sim.start_sem = xSemaphoreCreateBinary();

    /* 3. Driver setup */
    gpio_setup(&g_dht22_sim.pin, GPIO_PORT, GPIO_PIN, GPIO_Mode_IPU, GPIO_Speed_50MHz);
    gpio_enable(&g_dht22_sim.pin);

    timer_setup(&g_dht22_sim.ic, TIM_IC_INST);
    timer_setup(&g_dht22_sim.oc, TIM_OC_INST);
    timer_register_callback(&g_dht22_sim.ic, prv_ic_cb, &g_dht22_sim);
    timer_register_callback(&g_dht22_sim.oc, prv_oc_cb, &g_dht22_sim);

    uint16_t psc = (uint16_t)((SystemCoreClock / 1000000u) - 1u);
    uint32_t arr = 0xFFFFu;

    timer_base_setup(&g_dht22_sim.ic, psc, arr);
    timer_base_setup(&g_dht22_sim.oc, psc, arr);

    timer_ic_setup(&g_dht22_sim.ic, TIMER_CH1, psc, arr, TIM_ICPolarity_Falling, 0x0Fu);
    timer_oc_setup(&g_dht22_sim.oc, TIMER_CH1, psc, arr, 0, TIM_OCMode_Timing);

    /* 4. Start task */
    xTaskCreate(prv_task, "Sim_DHT22", STACK_SIZE, &g_dht22_sim, TASK_PRIO, &g_dht22_sim.task_handle);

    timer_ic_enable(&g_dht22_sim.ic, TIMER_CH1);
    timer_enable(&g_dht22_sim.ic);
    timer_enable(&g_dht22_sim.oc);
}

/* IRQ Handlers ------------------------------------------------------------*/

void TIM4_IRQHandler(void) { timer_handle_irq(&g_dht22_sim.ic); }
void TIM5_IRQHandler(void) { timer_handle_irq(&g_dht22_sim.oc); }

/* Private functions -------------------------------------------------------*/

static void prv_task(void *arg)
{
    sim_dht22_t *sim = (sim_dht22_t *)arg;
    for (;;)
    {
        xSemaphoreTake(sim->start_sem, portMAX_DELAY);
        dht22_reset_comm(&sim->logic);
        timer_ic_disable(&sim->ic, TIMER_CH1);
        prv_set_pin_output();
        g_state = RESP_START_LOW;
        GPIO_WriteBit(GPIO_PORT, GPIO_PIN, Bit_RESET);
        prv_schedule_oc(DHT_RESP_LOW_US);
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
}

static void prv_oc_cb(void *ctx, const timer_evt_t *evt)
{
    sim_dht22_t *sim = (sim_dht22_t *)ctx;
    BaseType_t woken = pdFALSE;
    if (evt == NULL || evt->type != TIMER_EVT_COMPARE) return;

    switch (g_state)
    {
        case RESP_START_LOW:
            GPIO_WriteBit(GPIO_PORT, GPIO_PIN, Bit_SET);
            g_state = RESP_START_HIGH;
            prv_schedule_oc(DHT_RESP_HIGH_US);
            break;
        case RESP_START_HIGH:
            GPIO_WriteBit(GPIO_PORT, GPIO_PIN, Bit_RESET);
            g_state = RESP_BIT_LOW;
            prv_schedule_oc(DHT_BIT_LOW_US);
            break;
        case RESP_BIT_LOW:
            GPIO_WriteBit(GPIO_PORT, GPIO_PIN, Bit_SET);
            g_state = RESP_BIT_HIGH;
            uint8_t bit = dht22_tx(&sim->logic);
            prv_schedule_oc(bit ? DHT_BIT1_HIGH_US : DHT_BIT0_HIGH_US);
            break;
        case RESP_BIT_HIGH:
            if (sim->logic.current_bit < 40) {
                GPIO_WriteBit(GPIO_PORT, GPIO_PIN, Bit_RESET);
                g_state = RESP_BIT_LOW;
                prv_schedule_oc(DHT_BIT_LOW_US);
            } else {
                GPIO_WriteBit(GPIO_PORT, GPIO_PIN, Bit_RESET);
                g_state = RESP_STOP_LOW;
                prv_schedule_oc(DHT_END_LOW_US);
            }
            break;
        case RESP_STOP_LOW:
            GPIO_WriteBit(GPIO_PORT, GPIO_PIN, Bit_SET);
            g_state = RESP_DONE;
            prv_schedule_oc(10);
            break;
        case RESP_DONE:
            timer_oc_disable(&sim->oc, TIMER_CH1);
            g_state = RESP_IDLE;
            prv_set_pin_input();
            timer_ic_enable(&sim->ic, TIMER_CH1);
            vTaskNotifyGiveFromISR(sim->task_handle, &woken);
            portYIELD_FROM_ISR(woken);
            break;
        default: break;
    }
}

static void prv_ic_cb(void *ctx, const timer_evt_t *evt)
{
    sim_dht22_t *sim = (sim_dht22_t *)ctx;
    BaseType_t woken = pdFALSE;
    if (evt == NULL || evt->type != TIMER_EVT_CAPTURE) return;

    if (g_ic_awaiting_rise == 0) {
        g_ic_fall_capture = evt->value;
        g_ic_awaiting_rise = 1;
        timer_ic_set_polarity(&sim->ic, TIMER_CH1, TIM_ICPolarity_Rising);
    } else {
        uint32_t t_rise = evt->value;
        uint32_t pulse = (t_rise >= g_ic_fall_capture) ? (t_rise - g_ic_fall_capture) : (0xFFFFu - g_ic_fall_capture + t_rise + 1);
        g_ic_awaiting_rise = 0;
        timer_ic_set_polarity(&sim->ic, TIMER_CH1, TIM_ICPolarity_Falling);
        if (pulse >= DHT_HOST_MIN_LOW_US) {
            xSemaphoreGiveFromISR(sim->start_sem, &woken);
            portYIELD_FROM_ISR(woken);
        }
    }
}

static void prv_set_pin_output(void)
{
    GPIO_InitTypeDef gpio_init;
    gpio_init.GPIO_Pin   = GPIO_PIN;
    gpio_init.GPIO_Mode  = GPIO_Mode_Out_OD;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIO_PORT, &gpio_init);
}

static void prv_set_pin_input(void)
{
    GPIO_InitTypeDef gpio_init;
    gpio_init.GPIO_Pin   = GPIO_PIN;
    gpio_init.GPIO_Mode  = GPIO_Mode_IPU;
    GPIO_Init(GPIO_PORT, &gpio_init);
}

static void prv_schedule_oc(uint16_t us)
{
    uint32_t cnt;
    timer_get_counter(&g_dht22_sim.oc, &cnt);
    timer_oc_set_compare(&g_dht22_sim.oc, TIMER_CH1, (uint16_t)(cnt + us));
    timer_oc_enable(&g_dht22_sim.oc, TIMER_CH1);
}
