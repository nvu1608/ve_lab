/**
 * @file    driver_common.h
 * @brief   Common definitions for all drivers
 * @details Provides shared status codes, utility macros, and common types
 *          used across the driver layer
 */

#ifndef DRIVER_COMMON_H
#define DRIVER_COMMON_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Includes ----------------------------------------------------------------*/

#include <stdint.h>
#include <stdbool.h>

/* Exported types ----------------------------------------------------------*/

/**
 * @brief   Driver status codes
 */
typedef enum
{
    DRIVER_OK = 0,             /**< Operation successful */
    DRIVER_ERR,                /**< General error */
    DRIVER_ERR_BUSY,           /**< Resource busy */
    DRIVER_ERR_TIMEOUT,        /**< Timeout occurred */
    DRIVER_ERR_INVALID_ARG,    /**< Invalid argument */
    DRIVER_ERR_UNSUPPORTED     /**< Unsupported feature */
} driver_status_t;

#ifdef __cplusplus
}
#endif

#endif