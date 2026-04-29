#ifndef DRIVER_RCC_H
#define DRIVER_RCC_H

#include "stm32f10x.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{
    RCC_BUS_APB1 = 0,
    RCC_BUS_APB2,
    RCC_BUS_AHB
} rcc_bus_t;

void rcc_enable(rcc_bus_t bus, uint32_t peripheral);
void rcc_disable(rcc_bus_t bus, uint32_t peripheral);

void rcc_reset_enable(rcc_bus_t bus, uint32_t peripheral);
void rcc_reset_disable(rcc_bus_t bus, uint32_t peripheral);

#ifdef __cplusplus
}
#endif

#endif