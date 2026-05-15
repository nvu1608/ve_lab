/**
 * @file    dht11.h
 * @brief   DHT11 Pure Logic Simulation (No RTOS, No Driver).
 */

#ifndef DHT11_H
#define DHT11_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ----------------------------------------------------------------*/
#include <stdint.h>

/* Exported types ----------------------------------------------------------*/

/**
 * @brief DHT11 logic state.
 */
typedef struct {
    uint8_t humidity;    /**< Humidity value (percentage) */
    uint8_t temperature; /**< Temperature value (Celsius) */
    uint8_t frame[5];    /**< 40-bit data packet */
    uint8_t current_bit; /**< Bit index (0-39) */
} dht11_t;

/* Public API --------------------------------------------------------------*/

/**
 * @brief Initialize DHT11 logic with default values.
 */
void dht11_reset(dht11_t *dev);

/**
 * @brief Set simulated humidity and temperature values.
 */
void dht11_set(dht11_t *dev, uint8_t humidity, uint8_t temperature);

/**
 * @brief Get the value of the next bit to transmit.
 * @return 0 or 1.
 */
uint8_t dht11_tx(dht11_t *dev);

/**
 * @brief Reset communication state (e.g. on start signal).
 */
void dht11_reset_comm(dht11_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* DHT11_H */
