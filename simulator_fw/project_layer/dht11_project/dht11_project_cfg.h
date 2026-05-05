#ifndef DHT11_PROJECT_CFG_H
#define DHT11_PROJECT_CFG_H

#include "stm32f10x.h"

/* ============================================================
 * DHT11 Hardware Resource Mapping
 * ============================================================ */

/* GPIO Configuration (Single-bus) */
#define DHT11_GPIO_PORT             GPIOA
#define DHT11_GPIO_PIN              GPIO_Pin_0
#define DHT11_RCC_GPIO              RCC_APB2Periph_GPIOA
#define DHT11_RCC_AFIO              RCC_APB2Periph_AFIO

/* Timer Configuration */
#define DHT11_TIM_TX_INSTANCE       TIM1
#define DHT11_RCC_TIM_TX            RCC_APB2Periph_TIM1
#define DHT11_IRQ_TIM_TX            TIM1_CC_IRQn

#define DHT11_TIM_RX_INSTANCE       TIM2
#define DHT11_RCC_TIM_RX            RCC_APB1Periph_TIM2
#define DHT11_IRQ_TIM_RX            TIM2_IRQn

/* IRQ Priority */
#define DHT11_IRQ_PRIO              5

/* ============================================================
 * FreeRTOS Task Configuration
 * ============================================================ */
#define DHT11_TASK_PRIO             3
#define DHT11_TASK_STACK            256

#endif /* DHT11_PROJECT_CFG_H */
