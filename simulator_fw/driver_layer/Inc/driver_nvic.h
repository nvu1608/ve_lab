#ifndef DRIVER_NVIC_H
#define DRIVER_NVIC_H

#include "stm32f10x.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Set NVIC priority group
 */
static inline void nvic_set_priority_group(uint32_t priority_group)
{
    NVIC_PriorityGroupConfig(priority_group);
}

/**
 * @brief Enable IRQ with priority
 */
static inline void nvic_enable_irq(IRQn_Type irq,
                                   uint8_t preempt_priority,
                                   uint8_t sub_priority)
{
    NVIC_InitTypeDef nvic;

    nvic.NVIC_IRQChannel = irq;
    nvic.NVIC_IRQChannelPreemptionPriority = preempt_priority;
    nvic.NVIC_IRQChannelSubPriority = sub_priority;
    nvic.NVIC_IRQChannelCmd = ENABLE;

    NVIC_Init(&nvic);
}

/**
 * @brief Disable IRQ
 */
static inline void nvic_disable_irq(IRQn_Type irq)
{
    NVIC_InitTypeDef nvic;

    nvic.NVIC_IRQChannel = irq;
    nvic.NVIC_IRQChannelPreemptionPriority = 0u;
    nvic.NVIC_IRQChannelSubPriority = 0u;
    nvic.NVIC_IRQChannelCmd = DISABLE;

    NVIC_Init(&nvic);
}

#ifdef __cplusplus
}
#endif

#endif /* DRIVER_NVIC_H */
