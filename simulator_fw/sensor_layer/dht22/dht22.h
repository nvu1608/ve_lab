/**
 * @file    dht22.h
 * @brief   DHT22 Pure Logic Simulation (No RTOS, No Driver).
 */

#ifndef DHT22_H
#define DHT22_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ----------------------------------------------------------------*/
#include <stdint.h>

/* Exported types ----------------------------------------------------------*/

/**
 * @brief DHT22 logic state.
 */
typedef struct {
    int16_t  temperature; /**< Value x 10 (e.g. 255 for 25.5C). Bit 15 is sign. */
    uint16_t humidity;    /**< Value x 10 (e.g. 600 for 60.0%RH). */
    uint8_t  frame[5];    /**< 40-bit data packet */
    uint8_t  current_bit; /**< Bit index (0-39) */
} dht22_t;

/* Public API --------------------------------------------------------------*/

/**
 * @brief Initialize DHT22 logic with default values.
 */
void dht22_reset(dht22_t *dev);

/**
 * @brief Set simulated humidity and temperature values.
 * @param humidity_x10 Humidity multiplied by 10.
 * @param temp_x10 Temperature multiplied by 10.
 */
void dht22_set(dht22_t *dev, uint16_t humidity_x10, int16_t temp_x10);

/**
 * @brief Get the value of the next bit to transmit.
 * @return 0 or 1.
 */
uint8_t dht22_tx(dht22_t *dev);

/**
 * @brief Reset communication state (e.g. on start signal).
 */
void dht22_reset_comm(dht22_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* DHT22_H */
