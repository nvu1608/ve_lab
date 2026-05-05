#ifndef DHT11_API_H
#define DHT11_API_H

#include <stdint.h>

/**
 * @brief DHT11 Data Structure
 */
typedef struct
{
    uint8_t humidity;
    uint8_t temperature;
} dht11_data_t;

typedef struct dht11 dht11_t;

/**
 * @brief Public API to set simulated DHT11 data
 */
void dht11_set_data(dht11_t *dev, const dht11_data_t *data);

#endif /* DHT11_API_H */
