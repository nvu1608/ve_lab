#ifndef SENSOR_DS1307_API_H
#define SENSOR_DS1307_API_H

#include <stdint.h>

/**
 * @brief DS1307 Time Structure (Binary Format)
 */
typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day_of_week;
    uint8_t date;
    uint8_t month;
    uint16_t year;
} ds1307_time_t;

/**
 * @brief Forward declaration for DS1307 object
 */
typedef struct ds1307 ds1307_t;

/**
 * @brief Public APIs for interacting with DS1307 sensor simulation
 */
void ds1307_set_time(ds1307_t *dev, const ds1307_time_t *time);
void ds1307_get_time(ds1307_t *dev, ds1307_time_t *time);
uint8_t ds1307_is_halted(ds1307_t *dev);

#endif /* SENSOR_DS1307_API_H */
