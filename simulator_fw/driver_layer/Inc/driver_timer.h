#ifndef DRIVER_TIMER_H
#define DRIVER_TIMER_H

#include "stm32f10x.h"
#include "driver_common.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    TIMER_CH1 = 0,
    TIMER_CH2 = 1,
    TIMER_CH3 = 2,
    TIMER_CH4 = 3,
} timer_channel_t;

typedef enum
{
    TIMER_MODE_NONE = 0,
    TIMER_MODE_BASE,
    TIMER_MODE_PWM,
    TIMER_MODE_IC,
    TIMER_MODE_OC,
} timer_mode_t;

typedef enum
{
    TIMER_IC_POL_RISING  = TIM_ICPolarity_Rising,
    TIMER_IC_POL_FALLING = TIM_ICPolarity_Falling,
    TIMER_IC_POL_BOTH    = TIM_ICPolarity_BothEdge,
} timer_ic_polarity_t;

typedef enum
{
    TIMER_OC_MODE_TIMING   = TIM_OCMode_Timing,
    TIMER_OC_MODE_ACTIVE   = TIM_OCMode_Active,
    TIMER_OC_MODE_INACTIVE = TIM_OCMode_Inactive,
    TIMER_OC_MODE_TOGGLE   = TIM_OCMode_Toggle,
    TIMER_OC_MODE_PWM1     = TIM_OCMode_PWM1,
    TIMER_OC_MODE_PWM2     = TIM_OCMode_PWM2,
} timer_oc_mode_t;

typedef void (*timer_callback_t)(void *ctx, timer_channel_t ch, uint32_t val);
typedef void (*timer_dma_callback_t)(void *ctx, timer_channel_t ch);

typedef struct
{
    uint16_t prescaler;
    uint32_t period;
    timer_callback_t callback;
    void *ctx;
} timer_base_cfg_t;

typedef struct
{
    uint16_t prescaler;
    uint32_t period;
    uint16_t pulse;
    uint8_t polarity;      /* TIM_OCPolarity_High / Low */
    uint8_t dma_enable;
    uint16_t *dma_buf;
    uint16_t dma_len;
    timer_dma_callback_t dma_callback;
    void *dma_ctx;
} timer_pwm_cfg_t;

typedef struct
{
    uint16_t prescaler;
    uint32_t period;
    timer_ic_polarity_t polarity;
    uint8_t filter;        /* 0x00 - 0x0F */
    timer_callback_t callback;
    void *ctx;
    uint8_t dma_enable;
    uint16_t *dma_buf;
    uint16_t dma_len;
    timer_dma_callback_t dma_callback;
    void *dma_ctx;
} timer_ic_cfg_t;

typedef struct
{
    uint16_t prescaler;
    uint32_t period;
    uint16_t pulse;
    timer_oc_mode_t oc_mode;
    uint8_t polarity;      /* TIM_OCPolarity_High / Low */
    timer_callback_t callback;
    void *ctx;
    uint8_t dma_enable;
    uint16_t *dma_buf;
    uint16_t dma_len;
    timer_dma_callback_t dma_callback;
    void *dma_ctx;
} timer_oc_cfg_t;

/**
 * @brief Timer Object Structure
 */
typedef struct
{
    TIM_TypeDef *instance;
    
    struct {
        timer_mode_t mode;
        timer_callback_t callback;
        void *ctx;
        timer_dma_callback_t dma_callback;
        void *dma_ctx;
        DMA_Channel_TypeDef *dma_channel;
    } channels[4];
} timer_t;

/* Public API */
driver_status_t timer_init(timer_t *dev, TIM_TypeDef *instance);
driver_status_t timer_base_init(timer_t *dev, const timer_base_cfg_t *cfg);
driver_status_t timer_pwm_init(timer_t *dev, timer_channel_t ch, const timer_pwm_cfg_t *cfg);
driver_status_t timer_ic_init(timer_t *dev, timer_channel_t ch, const timer_ic_cfg_t *cfg);
driver_status_t timer_oc_init(timer_t *dev, timer_channel_t ch, const timer_oc_cfg_t *cfg);

driver_status_t timer_start(timer_t *dev, timer_channel_t ch);
driver_status_t timer_stop(timer_t *dev, timer_channel_t ch);

driver_status_t timer_read(timer_t *dev, timer_channel_t ch, uint32_t *val);
driver_status_t timer_write(timer_t *dev, timer_channel_t ch, uint16_t val);

void timer_irq_handler(timer_t *dev);
void timer_dma_irq_handler(timer_t *dev, timer_channel_t ch);

#ifdef __cplusplus
}
#endif

#endif /* DRIVER_TIMER_H */
