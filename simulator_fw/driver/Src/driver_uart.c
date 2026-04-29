#include "driver_uart.h"
#include <string.h>

driver_status_t uart_init(uart_t *dev, USART_TypeDef *instance, uint32_t baudrate)
{
    if (dev == NULL || instance == NULL)
    {
        return DRIVER_ERR_INVALID_ARG;
    }

    memset(dev, 0, sizeof(*dev));

    dev->instance = instance;
    dev->baudrate = baudrate;
    dev->tx_state = UART_STATE_IDLE;
    dev->rx_state = UART_STATE_IDLE;
    
    return DRIVER_OK;
}

driver_status_t uart_start(uart_t *dev)
{
    USART_InitTypeDef usart_init_struct;

    if (dev == NULL || dev->instance == NULL)
    {
        return DRIVER_ERR_INVALID_ARG;
    }

    usart_init_struct.USART_BaudRate = dev->baudrate;
    usart_init_struct.USART_WordLength = USART_WordLength_8b;
    usart_init_struct.USART_StopBits = USART_StopBits_1;
    usart_init_struct.USART_Parity = USART_Parity_No;
    usart_init_struct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart_init_struct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

    USART_Init(dev->instance, &usart_init_struct);

    /* Bật ngắt RXNE (nhận) ngay từ đầu để luôn luôn lắng nghe byte tới */
    USART_ITConfig(dev->instance, USART_IT_RXNE, ENABLE);

    USART_Cmd(dev->instance, ENABLE);

    dev->rx_state = UART_STATE_BUSY;

    return DRIVER_OK;
}

driver_status_t uart_stop(uart_t *dev)
{
    if (dev == NULL || dev->instance == NULL)
    {
        return DRIVER_ERR_INVALID_ARG;
    }

    USART_ITConfig(dev->instance, USART_IT_RXNE, DISABLE);
    USART_ITConfig(dev->instance, USART_IT_TXE, DISABLE);

    USART_Cmd(dev->instance, DISABLE);
    USART_DeInit(dev->instance);

    dev->tx_state = UART_STATE_IDLE;
    dev->rx_state = UART_STATE_IDLE;

    return DRIVER_OK;
}

driver_status_t uart_send_async(uart_t *dev, const uint8_t *data, uint16_t length)
{
    if (dev == NULL || dev->instance == NULL || data == NULL || length == 0u)
    {
        return DRIVER_ERR_INVALID_ARG;
    }

    if (dev->tx_state == UART_STATE_BUSY)
    {
        return DRIVER_ERR_BUSY;
    }

    dev->tx_buffer = data;
    dev->tx_length = length;
    dev->tx_index = 0u;
    dev->tx_state = UART_STATE_BUSY;

    /* Kích hoạt ngắt TXE. Ngắt sẽ gọi hàm irq_handler để đẩy byte đầu tiên đi */
    USART_ITConfig(dev->instance, USART_IT_TXE, ENABLE);

    return DRIVER_OK;
}

driver_status_t uart_set_event_callback(uart_t *dev, uart_event_cb_t cb, void *ctx)
{
    if (dev == NULL)
    {
        return DRIVER_ERR_INVALID_ARG;
    }

    dev->event_cb = cb;
    dev->event_ctx = ctx;

    return DRIVER_OK;
}

void uart_irq_handler(uart_t *dev)
{
    uint8_t data;

    if (dev == NULL || dev->instance == NULL)
    {
        return;
    }

    /* Xử lý ngắt Nhận (RXNE) */
    if (USART_GetITStatus(dev->instance, USART_IT_RXNE) != RESET)
    {
        data = (uint8_t)(USART_ReceiveData(dev->instance) & 0xFF);
        
        if (dev->event_cb != NULL)
        {
            dev->event_cb(dev->event_ctx, UART_EVT_RX_READY, data);
        }
    }

    /* Xử lý ngắt Truyền (TXE) */
    if (USART_GetITStatus(dev->instance, USART_IT_TXE) != RESET)
    {
        if (dev->tx_index < dev->tx_length)
        {
            /* Vẫn còn dữ liệu để gửi */
            USART_SendData(dev->instance, dev->tx_buffer[dev->tx_index]);
            dev->tx_index++;
        }
        else
        {
            /* Đã gửi hết chuỗi, tắt ngắt TXE */
            USART_ITConfig(dev->instance, USART_IT_TXE, DISABLE);
            dev->tx_state = UART_STATE_IDLE;

            if (dev->event_cb != NULL)
            {
                dev->event_cb(dev->event_ctx, UART_EVT_TX_DONE, 0u);
            }
        }
    }
    
    /* Có thể mở rộng bắt ngắt lỗi Overrun (ORE), Framing Error (FE) ở đây */
}
