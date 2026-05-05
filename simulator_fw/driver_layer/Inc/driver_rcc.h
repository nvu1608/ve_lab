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

/**
 * @brief Enable peripheral clock
 */
static inline void rcc_enable(rcc_bus_t bus, uint32_t peripheral)
{
    switch (bus)
    {
        case RCC_BUS_APB1: RCC_APB1PeriphClockCmd(peripheral, ENABLE); break;
        case RCC_BUS_APB2: RCC_APB2PeriphClockCmd(peripheral, ENABLE); break;
        case RCC_BUS_AHB:  RCC_AHBPeriphClockCmd(peripheral, ENABLE); break;
        default: break;
    }
}

/**
 * @brief Disable peripheral clock
 */
static inline void rcc_disable(rcc_bus_t bus, uint32_t peripheral)
{
    switch (bus)
    {
        case RCC_BUS_APB1: RCC_APB1PeriphClockCmd(peripheral, DISABLE); break;
        case RCC_BUS_APB2: RCC_APB2PeriphClockCmd(peripheral, DISABLE); break;
        case RCC_BUS_AHB:  RCC_AHBPeriphClockCmd(peripheral, DISABLE); break;
        default: break;
    }
}

/**
 * @brief Enable peripheral reset
 */
static inline void rcc_reset_enable(rcc_bus_t bus, uint32_t peripheral)
{
    switch (bus)
    {
        case RCC_BUS_APB1: RCC_APB1PeriphResetCmd(peripheral, ENABLE); break;
        case RCC_BUS_APB2: RCC_APB2PeriphResetCmd(peripheral, ENABLE); break;
        default: break;
    }
}

/**
 * @brief Disable peripheral reset
 */
static inline void rcc_reset_disable(rcc_bus_t bus, uint32_t peripheral)
{
    switch (bus)
    {
        case RCC_BUS_APB1: RCC_APB1PeriphResetCmd(peripheral, DISABLE); break;
        case RCC_BUS_APB2: RCC_APB2PeriphResetCmd(peripheral, DISABLE); break;
        default: break;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* DRIVER_RCC_H */
