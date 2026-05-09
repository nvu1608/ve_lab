/**
 * @file    mq2_sim.c
 * @brief   MQ-2 gas sensor simulation implementation.
 * @details Implements scene-based gas simulation, analog voltage ramping,
 *          output noise generation, and DAC output update task.
 */

#include <stdlib.h>
#include <string.h>

#include "mq2_sim.h"

/* Private macros ----------------------------------------------------------*/

#define MQ2_OUTPUT_MIN_MV           0u
#define MQ2_OUTPUT_MAX_MV           3300u

#define MQ2_SIM_UPDATE_MS           100u
#define MQ2_SIM_STEP_MV             26u

#define MQ2_NOISE_MV_MIN            (-15)
#define MQ2_NOISE_MV_MAX            (15)

/* Private function prototypes ---------------------------------------------*/

/**
 * @brief Main MQ-2 simulator task.
 * @details Periodically moves the current output voltage toward target voltage,
 *          adds small analog-like noise, and writes the result to DAC.
 */
static void prv_sim_task(void *argument);

/**
 * @brief Convert MQ-2 simulation scene to target output voltage.
 * @details The returned values are simplified voltage levels used by the
 *          simulator to represent clean air and gas concentration scenes.
 */
static uint16_t prv_scene_to_millivolts(mq2_scene_t scene);

/**
 * @brief Clamp voltage to valid MQ-2 simulator output range.
 */
static uint16_t prv_clamp_millivolts(uint16_t millivolts);

/**
 * @brief Generate pseudo-random analog noise in millivolts.
 * @details Noise is added around the simulated output voltage so ADC sampling
 *          does not produce an unrealistically constant value.
 */
static int32_t prv_generate_noise_mv(void);

/* Public functions --------------------------------------------------------*/

void mq2_set_scene(mq2_t *dev,
                   mq2_scene_t scene)
{
    if (dev == NULL)
    {
        return;
    }

    if (scene > MQ2_SCENE_HIGH_GAS)
    {
        return;
    }

    if (xSemaphoreTake(dev->mutex, portMAX_DELAY) == pdTRUE)
    {
        dev->scene = scene;
        dev->target_mv = prv_scene_to_millivolts(scene);

        xSemaphoreGive(dev->mutex);
    }
}

void mq2_set_target_millivolts(mq2_t *dev,
                               uint16_t millivolts)
{
    if (dev == NULL)
    {
        return;
    }

    if (xSemaphoreTake(dev->mutex, portMAX_DELAY) == pdTRUE)
    {
        dev->target_mv = prv_clamp_millivolts(millivolts);

        xSemaphoreGive(dev->mutex);
    }
}

uint16_t mq2_get_output_millivolts(const mq2_t *dev)
{
    uint16_t output_mv = 0u;

    if (dev == NULL)
    {
        return 0u;
    }

    if (xSemaphoreTake(dev->mutex, portMAX_DELAY) == pdTRUE)
    {
        output_mv = dev->output_mv;

        xSemaphoreGive(dev->mutex);
    }

    return output_mv;
}

mq2_scene_t mq2_get_scene(const mq2_t *dev)
{
    mq2_scene_t scene = MQ2_SCENE_CLEAN_AIR;

    if (dev == NULL)
    {
        return scene;
    }

    if (xSemaphoreTake(dev->mutex, portMAX_DELAY) == pdTRUE)
    {
        scene = dev->scene;

        xSemaphoreGive(dev->mutex);
    }

    return scene;
}

void mq2_sim_start(mq2_t *dev,
                   dac_t *dac,
                   uint32_t task_priority,
                   uint32_t stack_size)
{
    if (dev == NULL || dac == NULL)
    {
        return;
    }

    memset(dev, 0, sizeof(mq2_t));

    dev->dac = dac;

    dev->mutex = xSemaphoreCreateMutex();

    if (dev->mutex == NULL)
    {
        return;
    }

     /*
     * Default simulation behavior:
     * - Start output at clean-air voltage.
     * - Target high-gas voltage.
     * - The simulation task ramps output gradually toward target.
     */
    dev->scene = MQ2_SCENE_HIGH_GAS;
    dev->output_mv = prv_scene_to_millivolts(MQ2_SCENE_CLEAN_AIR);
    dev->target_mv = prv_scene_to_millivolts(MQ2_SCENE_HIGH_GAS);

    (void)dac_write_millivolts(dev->dac, dev->output_mv);

    (void)xTaskCreate(prv_sim_task,
                      "MQ2_Sim",
                      (uint16_t)stack_size,
                      dev,
                      task_priority,
                      &dev->task_handle);
}

/* Private functions -------------------------------------------------------*/

static void prv_sim_task(void *argument)
{
    mq2_t *dev = (mq2_t *)argument;

    while (1)
    {
        if (xSemaphoreTake(dev->mutex, portMAX_DELAY) == pdTRUE)
        {
            if (dev->output_mv < dev->target_mv)
            {
                uint16_t delta = dev->target_mv - dev->output_mv;

                dev->output_mv += (delta > MQ2_SIM_STEP_MV)
                                      ? MQ2_SIM_STEP_MV
                                      : delta;
            }
            else if (dev->output_mv > dev->target_mv)
            {
                uint16_t delta = dev->output_mv - dev->target_mv;

                dev->output_mv -= (delta > MQ2_SIM_STEP_MV)
                                      ? MQ2_SIM_STEP_MV
                                      : delta;
            }

            /*
             * Add small random noise so ADC readings look closer to a real
             * analog sensor instead of a perfectly stable voltage.
             */
            int32_t noisy_output =
                (int32_t)dev->output_mv + prv_generate_noise_mv();

            if (noisy_output < MQ2_OUTPUT_MIN_MV)
            {
                noisy_output = MQ2_OUTPUT_MIN_MV;
            }

            if (noisy_output > MQ2_OUTPUT_MAX_MV)
            {
                noisy_output = MQ2_OUTPUT_MAX_MV;
            }

            (void)dac_write_millivolts(dev->dac,
                                       (uint16_t)noisy_output);

            xSemaphoreGive(dev->mutex);
        }

        vTaskDelay(pdMS_TO_TICKS(MQ2_SIM_UPDATE_MS));
    }
}

static uint16_t prv_scene_to_millivolts(mq2_scene_t scene)
{
    switch (scene)
    {
        case MQ2_SCENE_CLEAN_AIR:
            return 400u;

        case MQ2_SCENE_LOW_GAS:
            return 1000u;

        case MQ2_SCENE_MEDIUM_GAS:
            return 2000u;

        case MQ2_SCENE_HIGH_GAS:
            return 3000u;

        default:
            return 400u;
    }
}

static uint16_t prv_clamp_millivolts(uint16_t millivolts)
{
    if (millivolts > MQ2_OUTPUT_MAX_MV)
    {
        return MQ2_OUTPUT_MAX_MV;
    }

    return millivolts;
}


static int32_t prv_generate_noise_mv(void)
{
    return (rand() %
           (MQ2_NOISE_MV_MAX - MQ2_NOISE_MV_MIN + 1))
           + MQ2_NOISE_MV_MIN;
}