#ifndef DRIVER_I2C_SLAVE_H
#define DRIVER_I2C_SLAVE_H

#include "stm32f10x.h"
#include "driver_common.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{
    I2C_SLAVE_STATE_IDLE = 0,
    I2C_SLAVE_STATE_RX,
    I2C_SLAVE_STATE_TX    
} i2c_slave_state_t;

typedef enum
{
    I2C_SLAVE_EVT_WRITE_START = 0,
    I2C_SLAVE_EVT_READ_START,
    I2C_SLAVE_EVT_BYTE_RECEIVED,
    I2C_SLAVE_EVT_BYTE_SENT,
    I2C_SLAVE_EVT_WRITE_DONE,
    I2C_SLAVE_EVT_READ_DONE,
    I2C_SLAVE_EVT_ERROR
} i2c_slave_event_t;

typedef struct 
{
    i2c_slave_event_t type;
    uint8_t data;
    uint32_t error;
} i2c_slave_evt_t;

typedef struct i2c_slave i2c_slave_t;

typedef void (*i2c_slave_event_cb_t) (void *ctx,
                                      const i2c_slave_evt_t *evt);

typedef uint8_t (*i2c_slave_tx_byte_cb_t)(void *ctx);

/**
 * @brief I2C Slave Object Structure
 */
struct i2c_slave
{
    I2C_TypeDef *instance;          /* Pointer to I2C peripheral registers (e.g., I2C1) */

    uint32_t clock_speed;           /* I2C clock speed in Hz */
    uint16_t own_address;           /* 7-bit slave address */
    uint16_t duty_cycle;            /* I2C duty cycle (e.g., I2C_DutyCycle_2) */

    volatile i2c_slave_state_t state;

    /* Callback for events */
    i2c_slave_event_cb_t event_cb;
    void *event_ctx;

    /* Callback for getting TX data */
    i2c_slave_tx_byte_cb_t tx_byte_cb;
    void *tx_byte_ctx;
};

/* API Functions */
driver_status_t i2c_slave_init(i2c_slave_t *dev,
                               I2C_TypeDef *instance,
                               uint16_t own_address,
                               uint32_t clock_speed,
                               uint16_t duty_cycle);

driver_status_t i2c_slave_start(i2c_slave_t *dev);
driver_status_t i2c_slave_stop(i2c_slave_t *dev);

driver_status_t i2c_slave_set_event_callback(i2c_slave_t *dev,
                                             i2c_slave_event_cb_t cb,
                                             void *ctx);

driver_status_t i2c_slave_set_tx_byte_callback(i2c_slave_t *dev,
                                               i2c_slave_tx_byte_cb_t cb,
                                               void *ctx);

void i2c_slave_ev_irq_handler(i2c_slave_t *dev);
void i2c_slave_er_irq_handler(i2c_slave_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* DRIVER_I2C_SLAVE_H */
