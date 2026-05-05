#ifndef DRIVER_GPIO_H
#define DRIVER_GPIO_H

#include "stm32f10x.h"
#include "driver_common.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{
    GPIO_STATE_IDLE = 0,
    GPIO_STATE_READY
} gpio_state_t;

typedef enum
{
    GPIO_EXTI_DISABLE = 0,
    GPIO_EXTI_ENABLE
} gpio_exti_state_t;

typedef void (*gpio_exti_cb_t)(void *ctx);

/**
 * @brief GPIO Object Structure
 */
typedef struct
{
    GPIO_TypeDef *instance;         /* Pointer to GPIO peripheral registers (e.g., GPIOA) */
    uint16_t pin;                   /* GPIO pin mask (e.g., GPIO_Pin_0) */
    GPIOMode_TypeDef mode;          /* GPIO mode (e.g., GPIO_Mode_Out_PP) */
    GPIOSpeed_TypeDef speed;        /* GPIO speed (e.g., GPIO_Speed_50MHz) */

    volatile gpio_state_t state;

    /* EXTI configuration */
    gpio_exti_state_t exti_state;
    EXTITrigger_TypeDef exti_trigger;
    uint8_t exti_preempt_priority;
    uint8_t exti_sub_priority;

    /* EXTI Callback */
    gpio_exti_cb_t exti_cb;
    void *exti_ctx;
} gpio_t;

/* API Functions */
driver_status_t gpio_init(gpio_t *dev,
                          GPIO_TypeDef *instance,
                          uint16_t pin,
                          GPIOMode_TypeDef mode,
                          GPIOSpeed_TypeDef speed);

driver_status_t gpio_set_exti(gpio_t *dev,
                              EXTITrigger_TypeDef trigger,
                              uint8_t preempt_priority,
                              uint8_t sub_priority);

driver_status_t gpio_start(gpio_t *dev);
driver_status_t gpio_stop(gpio_t *dev);

uint8_t gpio_read(gpio_t *dev);
driver_status_t gpio_write(gpio_t *dev, uint8_t level);
driver_status_t gpio_set(gpio_t *dev);
driver_status_t gpio_reset(gpio_t *dev);
driver_status_t gpio_toggle(gpio_t *dev);

driver_status_t gpio_set_exti_callback(gpio_t *dev,
                                       gpio_exti_cb_t cb,
                                       void *ctx);

void gpio_exti_handler(gpio_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* DRIVER_GPIO_H */
