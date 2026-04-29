#ifndef DRIVER_NVIC_H
#define DRIVER_NVIC_H

#include "stm32f10x.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

void nvic_set_priority_group(uint32_t priority_group);

void nvic_enable_irq(IRQn_Type irq,
                     uint8_t preempt_priority,
                     uint8_t sub_priority);

void nvic_disable_irq(IRQn_Type irq);

#ifdef __cplusplus
}
#endif

#endif