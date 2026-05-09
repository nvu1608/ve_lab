/**
 * @file    mq2_api.h
 * @brief   MQ-2 gas sensor simulation public API.
 * @details Provides APIs to control simulated gas scenes and output voltage.
 */

#ifndef MQ2_API_H
#define MQ2_API_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Includes ----------------------------------------------------------------*/

#include <stdint.h>

/* Exported types ----------------------------------------------------------*/

/**
 * @brief MQ-2 gas simulation scene.
 */
typedef enum
{
    MQ2_SCENE_CLEAN_AIR = 0, /**< Clean air */
    MQ2_SCENE_LOW_GAS,      /**< Low gas concentration */
    MQ2_SCENE_MEDIUM_GAS,   /**< Medium gas concentration */
    MQ2_SCENE_HIGH_GAS      /**< High gas concentration */
} mq2_scene_t;

/**
 * @brief MQ-2 sensor object forward declaration.
 */
typedef struct mq2 mq2_t;

/* Public API --------------------------------------------------------------*/

/**
 * @brief Set MQ-2 simulation scene.
 *
 * @param dev Pointer to MQ-2 object.
 * @param scene Simulation scene.
 */
void mq2_set_scene(mq2_t *dev, mq2_scene_t scene);

/**
 * @brief Set target output voltage directly.
 *
 * @param dev Pointer to MQ-2 object.
 * @param millivolts Target output voltage in millivolts.
 */
void mq2_set_target_millivolts(mq2_t *dev, uint16_t millivolts);

/**
 * @brief Get current MQ-2 output voltage.
 *
 * @param dev Pointer to MQ-2 object.
 *
 * @return Current output voltage in millivolts.
 */
uint16_t mq2_get_output_millivolts(const mq2_t *dev);

/**
 * @brief Get current MQ-2 simulation scene.
 *
 * @param dev Pointer to MQ-2 object.
 *
 * @return Current simulation scene.
 */
mq2_scene_t mq2_get_scene(const mq2_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* MQ2_API_H */