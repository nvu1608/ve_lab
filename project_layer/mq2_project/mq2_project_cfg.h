/**
 * @file    mq2_project_cfg.h
 * @brief   MQ-2 simulation project configuration.
 * @details Defines DAC, GPIO, RCC, and FreeRTOS task configuration for
 *          the MQ-2 simulation project.
 */

#ifndef MQ2_PROJECT_CFG_H
#define MQ2_PROJECT_CFG_H

/* Includes ----------------------------------------------------------------*/

#include "stm32f10x.h"
#include "driver_dac.h"

/* DAC configuration -------------------------------------------------------*/

#define MQ2_DAC_INSTANCE            DAC
#define MQ2_DAC_CHANNEL             DAC_Channel_1
#define MQ2_DAC_TRIGGER             DAC_Trigger_None
#define MQ2_DAC_OUTPUT_BUFFER       DAC_OutputBuffer_Enable
#define MQ2_DAC_ALIGN               DAC_ALIGN_12B_R
#define MQ2_DAC_VREF_MV             3300u

/* GPIO configuration ------------------------------------------------------*/

#define MQ2_GPIO_PORT               GPIOA
#define MQ2_GPIO_DAC_PIN            GPIO_Pin_4
#define MQ2_GPIO_MODE               GPIO_Mode_AIN
#define MQ2_GPIO_SPEED              GPIO_Speed_50MHz

/* RCC configuration -------------------------------------------------------*/

#define MQ2_RCC_GPIO                RCC_APB2Periph_GPIOA
#define MQ2_RCC_DAC                 RCC_APB1Periph_DAC

/* FreeRTOS task configuration --------------------------------------------*/

#define MQ2_TASK_PRIO               2u
#define MQ2_TASK_STACK              256u

#endif /* MQ2_PROJECT_CFG_H */