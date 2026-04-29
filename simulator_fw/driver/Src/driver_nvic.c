#include "driver_nvic.h"

void nvic_set_priority_group(uint32_t priority_group)
{
    NVIC_PriorityGroupConfig(priority_group);
}

void nvic_enable_irq(IRQn_Type irq,
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

void nvic_disable_irq(IRQn_Type irq)
{
    NVIC_InitTypeDef nvic;

    nvic.NVIC_IRQChannel = irq;
    nvic.NVIC_IRQChannelPreemptionPriority = 0;
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = DISABLE;

    NVIC_Init(&nvic);
}