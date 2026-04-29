#include "driver_i2c_slave.h"
#include <string.h>

/* ====================================================================
 * PRIVATE FUNCTIONS
 * ==================================================================== */

/**
 * @brief Phát sự kiện lên tầng trên thông qua callback
 */
static void prv_i2c_slave_emit_event(i2c_slave_t *dev,
                                     i2c_slave_event_t type,
                                     uint8_t data,
                                     uint32_t error)
{
    if ((dev == NULL) || (dev->event_cb == NULL))
    {
        return;
    }

    i2c_slave_evt_t evt;
    evt.type = type;
    evt.data = data;
    evt.error = error;

    dev->event_cb(dev->event_ctx, &evt);
}

/**
 * @brief Lấy byte dữ liệu để gửi đi thông qua callback
 */
static uint8_t prv_i2c_slave_get_tx_byte(i2c_slave_t *dev)
{
    if ((dev == NULL) || (dev->tx_byte_cb == NULL))
    {
        return 0xFFu;
    }

    return dev->tx_byte_cb(dev->tx_byte_ctx);
}

/* ====================================================================
 * PUBLIC API
 * ==================================================================== */

driver_status_t i2c_slave_init(i2c_slave_t *dev,
                               I2C_TypeDef *instance,
                               uint16_t own_address,
                               uint32_t clock_speed,
                               uint16_t duty_cycle)
{
    if (dev == NULL)
    {
        return DRIVER_ERR_INVALID_ARG;
    }

    memset(dev, 0, sizeof(*dev));

    dev->instance = instance;
    dev->own_address = own_address;
    dev->clock_speed = clock_speed;
    dev->duty_cycle = duty_cycle;
    dev->state = I2C_SLAVE_STATE_IDLE;

    return DRIVER_OK;
}

driver_status_t i2c_slave_start(i2c_slave_t *dev)
{
    if ((dev == NULL) || (dev->instance == NULL))
    {
        return DRIVER_ERR_INVALID_ARG;
    }

    I2C_InitTypeDef i2c_init;

    i2c_init.I2C_Mode = I2C_Mode_I2C;
    i2c_init.I2C_ClockSpeed = dev->clock_speed;
    i2c_init.I2C_DutyCycle = dev->duty_cycle;
    i2c_init.I2C_OwnAddress1 = (uint16_t)(dev->own_address << 1);
    i2c_init.I2C_Ack = I2C_Ack_Enable;
    i2c_init.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;

    I2C_Init(dev->instance, &i2c_init);
    I2C_AcknowledgeConfig(dev->instance, ENABLE);   

    /* Bật interrupt sự kiện, buffer và lỗi */
    I2C_ITConfig(dev->instance, I2C_IT_EVT | I2C_IT_BUF | I2C_IT_ERR, ENABLE);

    I2C_Cmd(dev->instance, ENABLE);

    dev->state = I2C_SLAVE_STATE_IDLE;

    return DRIVER_OK;
}

driver_status_t i2c_slave_stop(i2c_slave_t *dev)
{
    if ((dev == NULL) || (dev->instance == NULL))
    {
        return DRIVER_ERR_INVALID_ARG;
    }

    I2C_ITConfig(dev->instance, I2C_IT_EVT | I2C_IT_BUF | I2C_IT_ERR, DISABLE);
    I2C_Cmd(dev->instance, DISABLE);

    dev->state = I2C_SLAVE_STATE_IDLE;

    return DRIVER_OK;
}

driver_status_t i2c_slave_set_event_callback(i2c_slave_t *dev,
                                             i2c_slave_event_cb_t cb,
                                             void *ctx)
{
    if (dev == NULL)
    {
        return DRIVER_ERR_INVALID_ARG;
    }

    dev->event_cb = cb;
    dev->event_ctx = ctx;

    return DRIVER_OK;
}

driver_status_t i2c_slave_set_tx_byte_callback(i2c_slave_t *dev,
                                               i2c_slave_tx_byte_cb_t cb,
                                               void *ctx)
{
    if (dev == NULL)
    {
        return DRIVER_ERR_INVALID_ARG;
    }

    dev->tx_byte_cb = cb;
    dev->tx_byte_ctx = ctx;

    return DRIVER_OK;
}

void i2c_slave_ev_irq_handler(i2c_slave_t *dev)
{
    if ((dev == NULL) || (dev->instance == NULL))
    {
        return;
    }

    I2C_TypeDef *i2c = dev->instance;
    uint16_t sr1 = i2c->SR1;

    /* ADDR: Address matched (Slave) */
    if ((sr1 & I2C_SR1_ADDR) != 0u)
    {
        uint16_t sr2 = i2c->SR2;

        if ((sr2 & I2C_SR2_TRA) != 0u)
        {
            /* Master muốn READ data từ Slave */
            uint8_t tx_data;

            dev->state = I2C_SLAVE_STATE_TX;

            prv_i2c_slave_emit_event(dev, I2C_SLAVE_EVT_READ_START, 0u, 0u);

            tx_data = prv_i2c_slave_get_tx_byte(dev);
            i2c->DR = tx_data;

            prv_i2c_slave_emit_event(dev, I2C_SLAVE_EVT_BYTE_SENT, tx_data, 0u);
        }
        else
        {
            /* Master muốn WRITE data vào Slave */
            dev->state = I2C_SLAVE_STATE_RX;

            prv_i2c_slave_emit_event(dev, I2C_SLAVE_EVT_WRITE_START, 0u, 0u);
        }

        return;
    }

    /* STOPF: Stop detection (Slave) */
    if ((sr1 & I2C_SR1_STOPF) != 0u)
    {
        /* Clear STOPF bằng cách đọc SR1 rồi ghi vào CR1 */
        (void)i2c->SR1;
        i2c->CR1 |= I2C_CR1_PE;

        if (dev->state == I2C_SLAVE_STATE_RX)
        {
            prv_i2c_slave_emit_event(dev, I2C_SLAVE_EVT_WRITE_DONE, 0u, 0u);
        }

        dev->state = I2C_SLAVE_STATE_IDLE;
        return;
    }

    /* RXNE: Receive buffer not empty */
    if ((sr1 & I2C_SR1_RXNE) != 0u)
    {
        uint8_t rx_data = (uint8_t)i2c->DR;

        if (dev->state != I2C_SLAVE_STATE_RX)
        {
            dev->state = I2C_SLAVE_STATE_RX;
        }

        prv_i2c_slave_emit_event(dev, I2C_SLAVE_EVT_BYTE_RECEIVED, rx_data, 0u);

        return;
    }

    /* TXE: Transmit buffer empty */
    if ((sr1 & I2C_SR1_TXE) != 0u)
    {
        uint8_t tx_data;

        if (dev->state != I2C_SLAVE_STATE_TX)
        {
            dev->state = I2C_SLAVE_STATE_TX;
        }

        tx_data = prv_i2c_slave_get_tx_byte(dev);
        i2c->DR = tx_data;

        prv_i2c_slave_emit_event(dev, I2C_SLAVE_EVT_BYTE_SENT, tx_data, 0u);

        return;
    }

    /* BTF: Byte transfer finished */
    if ((sr1 & I2C_SR1_BTF) != 0u)
    {
        if (dev->state == I2C_SLAVE_STATE_TX)
        {
            uint8_t tx_data = prv_i2c_slave_get_tx_byte(dev);
            i2c->DR = tx_data;

            prv_i2c_slave_emit_event(dev, I2C_SLAVE_EVT_BYTE_SENT, tx_data, 0u);
        }
        else if (dev->state == I2C_SLAVE_STATE_RX)
        {
            uint8_t rx_data = (uint8_t)i2c->DR;
            
            prv_i2c_slave_emit_event(dev, I2C_SLAVE_EVT_BYTE_RECEIVED, rx_data, 0u);
        }
        return;
    }
}

void i2c_slave_er_irq_handler(i2c_slave_t *dev)
{
    if ((dev == NULL) || (dev->instance == NULL))
    {
        return;
    }

    I2C_TypeDef *i2c = dev->instance;
    uint32_t error = 0u;

    /* AF: Acknowledge failure */
    if ((i2c->SR1 & I2C_SR1_AF) != 0u)
    {
        i2c->SR1 &= (uint16_t)~I2C_SR1_AF;

        if (dev->state == I2C_SLAVE_STATE_TX)
        {
            /* Master gửi NACK để báo kết thúc phiên READ */
            prv_i2c_slave_emit_event(dev, I2C_SLAVE_EVT_READ_DONE, 0u, 0u);
            dev->state = I2C_SLAVE_STATE_IDLE;
        }
        else
        {
            error |= I2C_SR1_AF;
        }
    }

    /* BERR: Bus error */
    if ((i2c->SR1 & I2C_SR1_BERR) != 0u)
    {
        i2c->SR1 &= (uint16_t)~I2C_SR1_BERR;
        error |= I2C_SR1_BERR;
    }

    /* ARLO: Arbitration lost */
    if ((i2c->SR1 & I2C_SR1_ARLO) != 0u)
    {
        i2c->SR1 &= (uint16_t)~I2C_SR1_ARLO;
        error |= I2C_SR1_ARLO;
    }

    /* OVR: Overrun/Underrun */
    if ((i2c->SR1 & I2C_SR1_OVR) != 0u)
    {
        i2c->SR1 &= (uint16_t)~I2C_SR1_OVR;
        error |= I2C_SR1_OVR;
    }

    if (error != 0u)
    {
        dev->state = I2C_SLAVE_STATE_IDLE;
        prv_i2c_slave_emit_event(dev, I2C_SLAVE_EVT_ERROR, 0u, error);
    }
}
