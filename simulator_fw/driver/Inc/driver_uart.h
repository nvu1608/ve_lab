#ifndef DRIVER_UART_H
#define DRIVER_UART_H

#include "stm32f10x.h"
#include <stdint.h>
#include <stddef.h>
#include "driver_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    UART_STATE_IDLE = 0,
    UART_STATE_BUSY
} uart_state_t;

typedef enum
{
    UART_EVT_RX_READY = 0,
    UART_EVT_TX_DONE,
    UART_EVT_ERROR
} uart_event_t;

typedef void (*uart_event_cb_t)(void *ctx, uart_event_t event, uint8_t data);

typedef struct
{
    USART_TypeDef *instance;
    uint32_t baudrate;

    volatile uart_state_t tx_state;
    volatile uart_state_t rx_state;

    const uint8_t *tx_buffer;
    uint16_t tx_length;
    volatile uint16_t tx_index;

    uart_event_cb_t event_cb;
    void *event_ctx;
} uart_t;

driver_status_t uart_init(uart_t *dev, USART_TypeDef *instance, uint32_t baudrate);
driver_status_t uart_start(uart_t *dev);
driver_status_t uart_stop(uart_t *dev);

driver_status_t uart_send_async(uart_t *dev, const uint8_t *data, uint16_t length);

driver_status_t uart_set_event_callback(uart_t *dev, uart_event_cb_t cb, void *ctx);

void uart_irq_handler(uart_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* DRIVER_UART_H */
