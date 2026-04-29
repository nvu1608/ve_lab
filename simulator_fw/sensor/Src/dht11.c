#include "dht11.h"
#include "driver_timer.h"
#include "driver_gpio.h"
#include "driver_rcc.h"
#include <string.h>

/* ================================================================
 *  DHT11 PROTOCOL TIMING (µs)
 * ================================================================ */
#define DHT_RESP_LOW      80u
#define DHT_RESP_HIGH     80u
#define DHT_BIT_LOW       50u
#define DHT_BIT_0_HIGH    26u
#define DHT_BIT_1_HIGH    70u
#define DHT_STOP_LOW      50u
#define DHT_STOP_CLEANUP  10u

/* Start signal từ host: LOW 18ms..30ms */
#define DHT_START_MIN  18000u
#define DHT_START_MAX  30000u

/* ================================================================
 *  TX STATE MACHINE
 * ================================================================ */
typedef enum {
    TX_IDLE = 0,
    TX_RESP_LOW,
    TX_RESP_HIGH,
    TX_BIT_LOW,
    TX_BIT_HIGH,
    TX_STOP_LOW,
    TX_DONE
} prv_TxState_t;

/* ================================================================
 *  MODULE STATE
 * ================================================================ */
static struct {
    /* FreeRTOS */
    TaskHandle_t   task_handle;
    SemaphoreHandle_t start_sem;   /* IC Callback → Task: start hợp lệ   */
    QueueHandle_t  data_queue;     /* set_data() → Task: update data */

    /* Config */
    DHT11_Sim_Cfg_t cfg;
    uint8_t         initialized;
    uint8_t         running;

    /* DHT11 data frame */
    uint8_t data[5];

    /* TX state machine */
    volatile prv_TxState_t tx_state;
    volatile uint8_t       bit_idx;

    /* IC state */
    volatile uint8_t  ic_edge;        /* 0=chờ falling, 1=chờ rising */
    volatile uint16_t ic_t_fall;

    /* Drivers */
    gpio_t  dht_pin;
    timer_t tim_tx;   /* TIM1 cho OC */
    timer_t tim_rx;   /* TIM2 cho IC */
} s;

/* Expose drivers cho stm32f10x_it.c */
timer_t *g_dht11_tim_tx = &s.tim_tx;
timer_t *g_dht11_tim_rx = &s.tim_rx;

/* ================================================================
 *  PRIVATE — HARDWARE HELPERS
 * ================================================================ */

static void prv_pa0_tx_mode(void)
{
    gpio_init(&s.dht_pin, GPIOA, GPIO_Pin_0, GPIO_Mode_Out_OD, GPIO_Speed_50MHz);
    gpio_set(&s.dht_pin);
}

static void prv_pa0_ic_mode(void)
{
    gpio_init(&s.dht_pin, GPIOA, GPIO_Pin_0, GPIO_Mode_IPU, GPIO_Speed_50MHz);
    s.ic_edge = 0;
    
    /* Cấu hình lại TIM2 để bắt Falling edge */
    TIM_ICInitTypeDef ic;
    ic.TIM_Channel = TIM_Channel_1;
    ic.TIM_ICPolarity = TIM_ICPolarity_Falling;
    ic.TIM_ICSelection = TIM_ICSelection_DirectTI;
    ic.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    ic.TIM_ICFilter = 0x0Fu;
    TIM_ICInit(TIM2, &ic);
    
    TIM_ClearITPendingBit(TIM2, TIM_IT_CC1);
}

static inline void prv_oc_schedule(uint16_t us)
{
    timer_write(&s.tim_tx, TIMER_CH1, (uint16_t)(TIM1->CNT + us));
    TIM_ClearITPendingBit(TIM1, TIM_IT_CC1);
    TIM_ITConfig(TIM1, TIM_IT_CC1, ENABLE);
}

static inline uint8_t prv_current_bit(void)
{
    uint8_t byte_idx = s.bit_idx >> 3;
    uint8_t shift    = 7u - (s.bit_idx & 0x07u);
    return (s.data[byte_idx] >> shift) & 0x01u;
}

static void prv_build_frame(uint8_t hum, uint8_t tmp)
{
    s.data[0] = hum;
    s.data[1] = 0;
    s.data[2] = tmp;
    s.data[3] = 0;
    s.data[4] = (s.data[0] + s.data[1] + s.data[2] + s.data[3]) & 0xFFu;
}

/* ================================================================
 *  DRIVER CALLBACKS
 * ================================================================ */

static void prv_dht11_tx_callback(void *ctx, timer_channel_t ch, uint32_t val)
{
    (void)ctx; (void)ch; (void)val;
    BaseType_t higher_woken = pdFALSE;

    switch (s.tx_state)
    {
        case TX_RESP_LOW:
            gpio_set(&s.dht_pin);
            s.tx_state = TX_RESP_HIGH;
            prv_oc_schedule(DHT_RESP_HIGH);
            break;

        case TX_RESP_HIGH:
            gpio_reset(&s.dht_pin);
            s.tx_state = TX_BIT_LOW;
            prv_oc_schedule(DHT_BIT_LOW);
            break;

        case TX_BIT_LOW:
            gpio_set(&s.dht_pin);
            s.tx_state = TX_BIT_HIGH;
            prv_oc_schedule(prv_current_bit() ? DHT_BIT_1_HIGH : DHT_BIT_0_HIGH);
            break;

        case TX_BIT_HIGH:
            s.bit_idx++;
            if (s.bit_idx < 40u)
            {
                gpio_reset(&s.dht_pin);
                s.tx_state = TX_BIT_LOW;
                prv_oc_schedule(DHT_BIT_LOW);
            }
            else
            {
                gpio_reset(&s.dht_pin);
                s.tx_state = TX_STOP_LOW;
                prv_oc_schedule(DHT_STOP_LOW);
            }
            break;

        case TX_STOP_LOW:
            gpio_set(&s.dht_pin);
            s.tx_state = TX_DONE;
            prv_oc_schedule(DHT_STOP_CLEANUP);
            break;

        case TX_DONE:
            TIM_ITConfig(TIM1, TIM_IT_CC1, DISABLE);
            s.tx_state = TX_IDLE;
            prv_pa0_ic_mode();
            TIM_SetCounter(TIM2, 0);
            timer_start(&s.tim_rx, TIMER_CH1);
            vTaskNotifyGiveFromISR(s.task_handle, &higher_woken);
            portYIELD_FROM_ISR(higher_woken);
            break;

        default:
            TIM_ITConfig(TIM1, TIM_IT_CC1, DISABLE);
            s.tx_state = TX_IDLE;
            break;
    }
}

static void prv_dht11_rx_callback(void *ctx, timer_channel_t ch, uint32_t val)
{
    (void)ctx; (void)ch;
    BaseType_t higher_woken = pdFALSE;

    if (s.ic_edge == 0)
    {
        s.ic_t_fall = (uint16_t)val;
        s.ic_edge   = 1;
        TIM2->CCER &= ~TIM_CCER_CC1P; /* Chuyển sang Rising edge */
    }
    else
    {
        uint16_t t_rise = (uint16_t)val;
        uint32_t pulse_us = (t_rise >= s.ic_t_fall)
                             ? (uint32_t)(t_rise - s.ic_t_fall)
                             : (uint32_t)(0xFFFFu - s.ic_t_fall + t_rise + 1u);
        s.ic_edge = 0;
        TIM2->CCER |= TIM_CCER_CC1P; /* Chuyển về Falling edge */

        if (pulse_us >= DHT_START_MIN && pulse_us <= DHT_START_MAX)
        {
            xSemaphoreGiveFromISR(s.start_sem, &higher_woken);
            portYIELD_FROM_ISR(higher_woken);
        }
    }
}

/* ================================================================
 *  DHT11 TASK
 * ================================================================ */
static void prv_dht11_task(void *arg)
{
    (void)arg;
    uint8_t cur_hum = s.cfg.init_data.humidity;
    uint8_t cur_tmp = s.cfg.init_data.temperature;

    for (;;)
    {
        xSemaphoreTake(s.start_sem, portMAX_DELAY);

        DHT11_Data_t new_data;
        if (xQueueReceive(s.data_queue, &new_data, 0) == pdTRUE)
        {
            cur_hum = new_data.humidity;
            cur_tmp = new_data.temperature;
        }

        prv_build_frame(cur_hum, cur_tmp);

        /* Dừng RX, chuyển sang TX */
        timer_stop(&s.tim_rx, TIMER_CH1);
        prv_pa0_tx_mode();

        s.bit_idx  = 0;
        s.tx_state = TX_RESP_LOW;

        gpio_reset(&s.dht_pin);
        prv_oc_schedule(DHT_RESP_LOW);

        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
}

/* ================================================================
 *  PUBLIC API
 * ================================================================ */

static int prv_dht11_sim_config(const DHT11_Sim_Cfg_t *cfg)
{
    if (!cfg) return DHT11_SIM_EINVAL;

    s.cfg         = *cfg;
    s.initialized = 0;
    s.running     = 0;

    s.start_sem = xSemaphoreCreateBinary();
    s.data_queue = xQueueCreate(1, sizeof(DHT11_Data_t));

    /* 1. RCC */
    rcc_enable(RCC_BUS_APB2, RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO | RCC_APB2Periph_TIM1);
    rcc_enable(RCC_BUS_APB1, RCC_APB1Periph_TIM2);

    /* 2. GPIO */
    gpio_init(&s.dht_pin, GPIOA, GPIO_Pin_0, GPIO_Mode_IPU, GPIO_Speed_50MHz);

    /* 3. Timer TX (TIM1 OC) */
    timer_init(&s.tim_tx, TIM1);
    timer_oc_cfg_t oc_cfg = {
        .prescaler = (uint16_t)(SystemCoreClock / 1000000u),
        .period = 0xFFFFu,
        .pulse = 0,
        .oc_mode = TIMER_OC_MODE_TIMING,
        .callback = prv_dht11_tx_callback
    };
    timer_oc_init(&s.tim_tx, TIMER_CH1, &oc_cfg);

    /* 4. Timer RX (TIM2 IC) */
    timer_init(&s.tim_rx, TIM2);
    timer_ic_cfg_t ic_cfg = {
        .prescaler = (uint16_t)(SystemCoreClock / 1000000u),
        .period = 0xFFFFu,
        .polarity = TIMER_IC_POL_FALLING,
        .filter = 0x0Fu,
        .callback = prv_dht11_rx_callback
    };
    timer_ic_init(&s.tim_rx, TIMER_CH1, &ic_cfg);

    s.initialized = 1;
    return DHT11_SIM_OK;
}

static int prv_dht11_sim_run(void)
{
    if (!s.initialized)  return DHT11_SIM_ENOTCFG;
    if (s.running)       return DHT11_SIM_EBUSY;

    xTaskCreate(prv_dht11_task, "dht11_sim", s.cfg.task_stack, NULL, s.cfg.task_priority, &s.task_handle);

    timer_start(&s.tim_rx, TIMER_CH1);
    s.running = 1;
    return DHT11_SIM_OK;
}

void dht11_sim_init(void)
{
    DHT11_Sim_Cfg_t sim_cfg;
    sim_cfg.init_data.humidity    = 60;
    sim_cfg.init_data.temperature = 25;
    sim_cfg.task_priority         = 2u;
    sim_cfg.task_stack            = 128u;

    prv_dht11_sim_config(&sim_cfg);
    prv_dht11_sim_run();
}

int dht11_sim_set_data(const DHT11_Data_t *data)
{
    if (!data || !s.initialized) return DHT11_SIM_EINVAL;
    xQueueOverwrite(s.data_queue, data);
    return DHT11_SIM_OK;
}
