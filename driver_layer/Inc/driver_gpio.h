/**
 * @file    driver_gpio.h
 * @brief   GPIO driver interface.
 * @details Provides GPIO input/output and EXTI interrupt APIs.
 */

#ifndef DRIVER_GPIO_H
#define DRIVER_GPIO_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Includes ----------------------------------------------------------------*/

#include <stdint.h>
#include <stdbool.h>

#include "stm32f10x.h"
#include "driver_common.h"

/* Exported types ----------------------------------------------------------*/

/**
 * @brief GPIO driver state
 */

typedef enum
{
    GPIO_STATE_IDLE = 0,    /**< Driver is initialized but not started */
    GPIO_STATE_READY        /**< GPIO is configured and ready */
} gpio_state_t;

/**
 * @brief GPIO EXTI state.  
 */
typedef enum
{
    GPIO_EXTI_DISABLE = 0,  /**< EXTI disabled */
    GPIO_EXTI_ENABLE        /**< EXTI enabled */
} gpio_exti_state_t;

/**
 * @brief GPIO EXTI callback function type.
 *
 * @param ctx User context pointer.
 */
typedef void (*gpio_exti_cb_t)(void *ctx);

/**
 * @brief GPIO driver object.
 */
typedef struct
{
    GPIO_TypeDef *instance;                     /**< GPIO peripheral instance */
    uint16_t      pin;                          /**< GPIO pin mask */

    GPIOMode_TypeDef  mode;                     /**< GPIO mode */
    GPIOSpeed_TypeDef speed;                    /**< GPIO speed */

    volatile gpio_state_t state;                /**< Driver state */

    /* EXTI configuration --------------------------------------------------*/

    gpio_exti_state_t  exti_state;             /**< EXTI enable state */
    EXTITrigger_TypeDef exti_trigger;          /**< EXTI trigger type */
    uint8_t             exti_preempt_priority; /**< EXTI preemption priority */
    uint8_t             exti_sub_priority;     /**< EXTI sub priority */

    /* EXTI callback -------------------------------------------------------*/

    gpio_exti_cb_t exti_cb;                     /**< EXTI callback function */
    void          *exti_ctx;                    /**< User callback context */
} gpio_t;

/* Public API --------------------------------------------------------------*/

/**
 * @brief Initialize GPIO driver object.
 *
 * @param dev Pointer to GPIO driver object.
 * @param instance GPIO peripheral instance.
 * @param pin GPIO pin mask.
 * @param mode GPIO mode.
 * @param speed GPIO speed.
 *
 * @return DRIVER_OK if successful, otherwise an error code.
 */
driver_status_t gpio_init(gpio_t *dev,
                          GPIO_TypeDef *instance,
                          uint16_t pin,
                          GPIOMode_TypeDef mode,
                          GPIOSpeed_TypeDef speed);

/**
 * @brief Configure EXTI interrupt settings.
 *
 * @param dev Pointer to GPIO driver object.
 * @param trigger EXTI trigger type.
 * @param preempt_priority EXTI preemption priority.
 * @param sub_priority EXTI sub priority.
 *
 * @return DRIVER_OK if successful, otherwise an error code.
 */                          
driver_status_t gpio_set_exti(gpio_t *dev,
                              EXTITrigger_TypeDef trigger,
                              uint8_t preempt_priority,
                              uint8_t sub_priority);

/**
 * @brief Start GPIO peripheral.
 *
 * @param dev Pointer to GPIO driver object.
 *
 * @return DRIVER_OK if successful, otherwise an error code.
 */
driver_status_t gpio_start(gpio_t *dev);

/**
 * @brief Stop GPIO peripheral.
 *
 * @note This function deinitializes the entire GPIO peripheral port.
 *
 * @param dev Pointer to GPIO driver object.
 *
 * @return DRIVER_OK if successful, otherwise an error code.
 */
driver_status_t gpio_stop(gpio_t *dev);

/**
 * @brief Read GPIO input state.
 *
 * @param dev Pointer to GPIO driver object.
 *
 * @return GPIO logic level.
 */
bool gpio_read(gpio_t *dev);

/**
 * @brief Write GPIO output state.
 *
 * @param dev Pointer to GPIO driver object.
 * @param level GPIO logic level.
 *
 * @return DRIVER_OK if successful, otherwise an error code.
 */
driver_status_t gpio_write(gpio_t *dev, bool level);

/**
 * @brief Set GPIO output high.
 *
 * @param dev Pointer to GPIO driver object.
 *
 * @return DRIVER_OK if successful, otherwise an error code.
 */
driver_status_t gpio_set(gpio_t *dev);

/**
 * @brief Reset GPIO output low.
 *
 * @param dev Pointer to GPIO driver object.
 *
 * @return DRIVER_OK if successful, otherwise an error code.
 */
driver_status_t gpio_reset(gpio_t *dev);

/**
 * @brief Toggle GPIO output state.
 *
 * @param dev Pointer to GPIO driver object.
 *
 * @return DRIVER_OK if successful, otherwise an error code.
 */
driver_status_t gpio_toggle(gpio_t *dev);

/**
 * @brief Set EXTI callback function.
 *
 * @param dev Pointer to GPIO driver object.
 * @param cb EXTI callback function.
 * @param ctx User callback context.
 *
 * @return DRIVER_OK if successful, otherwise an error code.
 */
driver_status_t gpio_set_exti_callback(gpio_t *dev,
                                       gpio_exti_cb_t cb,
                                       void *ctx);

/**
 * @brief Process GPIO EXTI interrupt.
 *
 * @note Must be called from EXTI IRQ handler.
 *
 * @param dev Pointer to GPIO driver object.
 */
void gpio_exti_irq_process(gpio_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* DRIVER_GPIO_H */