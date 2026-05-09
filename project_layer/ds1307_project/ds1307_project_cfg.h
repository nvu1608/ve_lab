/**
 * @file    ds1307_project_cfg.h
 * @brief   DS1307 simulation project configuration.
 * @details Defines I2C, GPIO, RCC, NVIC, and FreeRTOS task configuration for
 *          the DS1307 simulation project.
 */

#ifndef DS1307_PROJECT_CFG_H
#define DS1307_PROJECT_CFG_H

/* Includes ----------------------------------------------------------------*/

#include "stm32f10x.h"

/* I2C configuration -------------------------------------------------------*/

#define DS1307_I2C_INSTANCE         I2C1
#define DS1307_I2C_ADDR             0x68u
#define DS1307_I2C_SPEED            100000u
#define DS1307_I2C_DUTY             I2C_DutyCycle_2

/* GPIO configuration ------------------------------------------------------*/

#define DS1307_GPIO_PORT            GPIOB
#define DS1307_GPIO_SCL_PIN         GPIO_Pin_6
#define DS1307_GPIO_SDA_PIN         GPIO_Pin_7
#define DS1307_GPIO_SPEED           GPIO_Speed_50MHz
#define DS1307_GPIO_MODE            GPIO_Mode_AF_OD

/* RCC configuration -------------------------------------------------------*/

#define DS1307_RCC_GPIO             RCC_APB2Periph_GPIOB
#define DS1307_RCC_AFIO             RCC_APB2Periph_AFIO
#define DS1307_RCC_I2C              RCC_APB1Periph_I2C1

/* NVIC configuration ------------------------------------------------------*/

#define DS1307_NVIC_PRIORITY_GROUP  NVIC_PriorityGroup_4
#define DS1307_IRQ_EV               I2C1_EV_IRQn
#define DS1307_IRQ_ER               I2C1_ER_IRQn
#define DS1307_IRQ_PREEMPT_PRIO     6u
#define DS1307_IRQ_SUB_PRIO         0u

/* FreeRTOS task configuration --------------------------------------------*/

#define DS1307_TASK_PRIO            2u
#define DS1307_TASK_STACK           256u

#endif /* DS1307_PROJECT_CFG_H */
