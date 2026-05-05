#include "dht11_sim.h"
#include "driver_gpio.h"
#include "driver_timer.h"
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

#define DHT_START_MIN     18000u
#define DHT_START_MAX     30000u

typedef enum {
    TX_IDLE = 0,
    TX_RESP_LOW,
    TX_RESP_HIGH,
    TX_BIT_LOW,
    TX_BIT_HIGH,
    TX_STOP_LOW,
    TX_DONE
} dht_tx_state_t;

/* ================================================================
 *  PRIVATE HELPERS
 * ================================================================ */

static void prv_set_pin_mode_tx(dht11_t *dev) {
    gpio_t *pin = (gpio_t *)dev->gpio_pin;
    gpio_init(pin, pin->instance, pin->pin, GPIO_Mode_Out_OD, GPIO_Speed_50MHz);
    gpio_start(pin);
    gpio_set(pin);
}

static void prv_set_pin_mode_rx(dht11_t *dev) {
    gpio_t *pin = (gpio_t *)dev->gpio_pin;
    gpio_init(pin, pin->instance, pin->pin, GPIO_Mode_IPU, GPIO_Speed_50MHz);
    gpio_start(pin);
    dev->ic_edge = 0; /* Waiting for Falling edge */
}

static void prv_oc_schedule(dht11_t *dev, uint16_t us) {
    timer_t *tim = (timer_t *)dev->tim_tx;
    uint32_t cnt;
    timer_read(tim, TIMER_CH1, &cnt);
    timer_write(tim, TIMER_CH1, (uint16_t)(cnt + us));
    TIM_ClearITPendingBit(tim->instance, TIM_IT_CC1);
    TIM_ITConfig(tim->instance, TIM_IT_CC1, ENABLE);
}

static uint8_t prv_current_bit(dht11_t *dev) {
    uint8_t byte_idx = dev->bit_idx >> 3;
    uint8_t shift    = 7u - (dev->bit_idx & 0x07u);
    return (dev->data[byte_idx] >> shift) & 0x01u;
}

static void prv_build_frame(dht11_t *dev, uint8_t hum, uint8_t tmp) {
    dev->data[0] = hum;
    dev->data[1] = 0;
    dev->data[2] = tmp;
    dev->data[3] = 0;
    dev->data[4] = (uint8_t)((dev->data[0] + dev->data[1] + dev->data[2] + dev->data[3]) & 0xFFu);
}

/* ================================================================
 *  CALLBACKS
 * ================================================================ */

static void prv_tx_callback(void *ctx, timer_channel_t ch, uint32_t val) {
    dht11_t *dev = (dht11_t *)ctx;
    (void)ch; (void)val;
    BaseType_t woken = pdFALSE;
    gpio_t *pin = (gpio_t *)dev->gpio_pin;
    timer_t *tim_tx = (timer_t *)dev->tim_tx;

    switch (dev->tx_state) {
        case TX_RESP_LOW:
            gpio_set(pin);
            dev->tx_state = TX_RESP_HIGH;
            prv_oc_schedule(dev, DHT_RESP_HIGH);
            break;

        case TX_RESP_HIGH:
            gpio_reset(pin);
            dev->tx_state = TX_BIT_LOW;
            prv_oc_schedule(dev, DHT_BIT_LOW);
            break;

        case TX_BIT_LOW:
            gpio_set(pin);
            dev->tx_state = TX_BIT_HIGH;
            prv_oc_schedule(dev, prv_current_bit(dev) ? DHT_BIT_1_HIGH : DHT_BIT_0_HIGH);
            break;

        case TX_BIT_HIGH:
            dev->bit_idx++;
            if (dev->bit_idx < 40u) {
                gpio_reset(pin);
                dev->tx_state = TX_BIT_LOW;
                prv_oc_schedule(dev, DHT_BIT_LOW);
            } else {
                gpio_reset(pin);
                dev->tx_state = TX_STOP_LOW;
                prv_oc_schedule(dev, DHT_STOP_LOW);
            }
            break;

        case TX_STOP_LOW:
            gpio_set(pin);
            dev->tx_state = TX_DONE;
            prv_oc_schedule(dev, DHT_STOP_CLEANUP);
            break;

        case TX_DONE:
            TIM_ITConfig(tim_tx->instance, TIM_IT_CC1, DISABLE);
            dev->tx_state = TX_IDLE;
            prv_set_pin_mode_rx(dev);
            
            /* Reset RX timer for next request */
            timer_t *tim_rx = (timer_t *)dev->tim_rx;
            TIM_SetCounter(tim_rx->instance, 0);
            timer_start(tim_rx, TIMER_CH1);
            
            vTaskNotifyGiveFromISR(dev->task_handle, &woken);
            break;
            
        default: break;
    }
    portYIELD_FROM_ISR(woken);
}

static void prv_rx_callback(void *ctx, timer_channel_t ch, uint32_t val) {
    dht11_t *dev = (dht11_t *)ctx;
    (void)ch;
    BaseType_t woken = pdFALSE;
    timer_t *tim_rx = (timer_t *)dev->tim_rx;

    if (dev->ic_edge == 0) {
        dev->ic_t_fall = (uint16_t)val;
        dev->ic_edge = 1;
        /* Switch to Rising edge detection */
        tim_rx->instance->CCER &= ~TIM_CCER_CC1P;
    } else {
        uint16_t t_rise = (uint16_t)val;
        uint32_t pulse_us = (t_rise >= dev->ic_t_fall)
                             ? (uint32_t)(t_rise - dev->ic_t_fall)
                             : (uint32_t)(0xFFFFu - dev->ic_t_fall + t_rise + 1u);
        dev->ic_edge = 0;
        /* Switch back to Falling edge detection */
        tim_rx->instance->CCER |= TIM_CCER_CC1P;

        if (pulse_us >= DHT_START_MIN && pulse_us <= DHT_START_MAX) {
            xSemaphoreGiveFromISR(dev->start_sem, &woken);
        }
    }
    portYIELD_FROM_ISR(woken);
}

/* ================================================================
 *  SIMULATION TASK
 * ================================================================ */

static void prv_sim_task(void *arg) {
    dht11_t *dev = (dht11_t *)arg;
    uint8_t hum = 50, tmp = 25;

    for (;;) {
        /* Wait for start signal (Master pulling LOW for 18ms+) */
        if (xSemaphoreTake(dev->start_sem, portMAX_DELAY) == pdTRUE) {
            dht11_data_t new_data;
            if (xQueueReceive(dev->data_queue, &new_data, 0) == pdTRUE) {
                hum = new_data.humidity;
                tmp = new_data.temperature;
            }

            prv_build_frame(dev, hum, tmp);

            /* Stop listening, start responding */
            timer_stop((timer_t *)dev->tim_rx, TIMER_CH1);
            prv_set_pin_mode_tx(dev);

            dev->bit_idx = 0;
            dev->tx_state = TX_RESP_LOW;

            gpio_reset((gpio_t *)dev->gpio_pin);
            prv_oc_schedule(dev, DHT_RESP_LOW);

            /* Wait for TX sequence to finish */
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        }
    }
}

/* ================================================================
 *  PUBLIC API
 * ================================================================ */

void dht11_set_data(dht11_t *dev, const dht11_data_t *data) {
    if (dev && data) {
        xQueueOverwrite(dev->data_queue, data);
    }
}

void dht11_sim_start(dht11_t *dev,
                     void *gpio_pin,
                     void *tim_tx,
                     void *tim_rx,
                     uint32_t task_priority,
                     uint32_t stack_size) {
    if (!dev || !gpio_pin || !tim_tx || !tim_rx) return;

    memset(dev, 0, sizeof(dht11_t));
    dev->gpio_pin = gpio_pin;
    dev->tim_tx = tim_tx;
    dev->tim_rx = tim_rx;

    dev->start_sem = xSemaphoreCreateBinary();
    dev->data_queue = xQueueCreate(1, sizeof(dht11_data_t));

    /* Register Callbacks */
    timer_oc_cfg_t oc_cfg = {
        .prescaler = (uint16_t)(SystemCoreClock / 1000000u),
        .period = 0xFFFFu,
        .pulse = 0,
        .oc_mode = TIMER_OC_MODE_TIMING,
        .callback = prv_tx_callback,
        .ctx = dev
    };
    timer_oc_init((timer_t *)dev->tim_tx, TIMER_CH1, &oc_cfg);

    timer_ic_cfg_t ic_cfg = {
        .prescaler = (uint16_t)(SystemCoreClock / 1000000u),
        .period = 0xFFFFu,
        .polarity = TIMER_IC_POL_FALLING,
        .filter = 0x0Fu,
        .callback = prv_rx_callback,
        .ctx = dev
    };
    timer_ic_init((timer_t *)dev->tim_rx, TIMER_CH1, &ic_cfg);

    /* Start Task */
    xTaskCreate(prv_sim_task, "DHT11_Sim", (uint16_t)stack_size, dev, task_priority, &dev->task_handle);

    /* Start RX Listener */
    prv_set_pin_mode_rx(dev);
    timer_start((timer_t *)dev->tim_rx, TIMER_CH1);
}
