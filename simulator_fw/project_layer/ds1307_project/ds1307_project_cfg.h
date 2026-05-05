#ifndef DS1307_PROJECT_CFG_H
#define DS1307_PROJECT_CFG_H

#include "stm32f10x.h"

/* ============================================================
 * DS1307 Hardware Resource Mapping
 * ============================================================ */

/* I2C Peripheral Configuration */
#define DS1307_I2C_INSTANCE         I2C1
#define DS1307_I2C_ADDR             0x68
#define DS1307_I2C_SPEED            100000
#define DS1307_I2C_DUTY             I2C_DutyCycle_2

/* GPIO Configuration (I2C1 Default: SCL=PB6, SDA=PB7) */
#define DS1307_GPIO_PORT            GPIOB
#define DS1307_GPIO_SCL_PIN         GPIO_Pin_6
#define DS1307_GPIO_SDA_PIN         GPIO_Pin_7

/* RCC Configuration */
#define DS1307_RCC_GPIO             RCC_APB2Periph_GPIOB
#define DS1307_RCC_I2C              RCC_APB1Periph_I2C1
#define DS1307_RCC_AFIO             RCC_APB2Periph_AFIO

/* NVIC Configuration */
#define DS1307_IRQ_EV               I2C1_EV_IRQn
#define DS1307_IRQ_ER               I2C1_ER_IRQn
#define DS1307_IRQ_PRIO             6

/* ============================================================
 * FreeRTOS Task Configuration
 * ============================================================ */
#define DS1307_TASK_PRIO            2
#define DS1307_TASK_STACK           256

#endif /* DS1307_PROJECT_CFG_H */
