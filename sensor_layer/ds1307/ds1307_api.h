/**
 * @file    ds1307_api.h
 * @brief   DS1307 sensor simulation public API.
 * @details Provides APIs to set, get, and check the simulated DS1307 RTC time.
 */

#ifndef DS1307_API_H
#define DS1307_API_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Includes ----------------------------------------------------------------*/

#include <stdint.h>

/* Exported types ----------------------------------------------------------*/

/**
 * @brief DS1307 time object in binary format.
 */
typedef struct
{
    uint8_t  second;      /**< Seconds, range 0-59 */
    uint8_t  minute;      /**< Minutes, range 0-59 */
    uint8_t  hour;        /**< Hours, range 0-23 */
    uint8_t  day_of_week; /**< Day of week, range 1-7 */
    uint8_t  date;        /**< Date of month, range 1-31 */
    uint8_t  month;       /**< Month, range 1-12 */
    uint16_t year;        /**< Full year, for example 2026 */
} ds1307_time_t;

/**
 * @brief DS1307 sensor object forward declaration.
 */
typedef struct ds1307 ds1307_t;

/* Public API --------------------------------------------------------------*/

/**
 * @brief Set simulated DS1307 time.
 *
 * @param dev Pointer to DS1307 object.
 * @param time Pointer to time object.
 */
void ds1307_set_time(ds1307_t *dev, const ds1307_time_t *time);

/**
 * @brief Get simulated DS1307 time.
 *
 * @param dev Pointer to DS1307 object.
 * @param time Pointer to output time object.
 */
void ds1307_get_time(ds1307_t *dev, ds1307_time_t *time);

/**
 * @brief Check whether DS1307 oscillator is halted.
 *
 * @param dev Pointer to DS1307 object.
 *
 * @return 1 if halted, otherwise 0.
 */
uint8_t ds1307_is_halted(const ds1307_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* DS1307_API_H */
