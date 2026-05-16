/**
 * @file    mq2_sim.h
 * @brief   MQ-2 gas sensor simulation object and control API.
 * @details Defines the MQ-2 simulator object, DAC backend, RTOS objects,
 *          simulation state, and API to start the simulation task.
 */

#ifndef MQ2_SIM_H
#define MQ2_SIM_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ----------------------------------------------------------------*/

#include <stdint.h>

#include "mq2_api.h"
#include "driver_dac.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

/* Exported types ----------------------------------------------------------*/

/**
 * @brief MQ-2 simulator object.
 */
struct mq2
{
    /* Peripheral interface -----------------------------------------------*/

    dac_t *dac; /**< DAC backend used to generate analog output voltage */

    /* RTOS objects --------------------------------------------------------*/

    SemaphoreHandle_t mutex;       /**< Protects shared simulation data */
    TaskHandle_t      task_handle; /**< Simulator task handle */

    /* Simulation state ----------------------------------------------------*/

    mq2_scene_t scene;     /**< Current gas simulation scene */
    uint16_t    target_mv; /**< Target output voltage in millivolts */
    uint16_t    output_mv; /**< Current output voltage in millivolts */
};

/* Public API --------------------------------------------------------------*/

/**
 * @brief Initialize and start MQ-2 simulation task.
 *
 * @param dev Pointer to MQ-2 simulator object.
 * @param dac Pointer to DAC driver object used as analog output backend.
 * @param task_priority FreeRTOS task priority.
 * @param stack_size FreeRTOS task stack size.
 */
void mq2_sim_start(mq2_t *dev,
                   dac_t *dac,
                   uint32_t task_priority,
                   uint32_t stack_size);

#ifdef __cplusplus

}
#endif

#endif  /* MQ2_SIM_H */