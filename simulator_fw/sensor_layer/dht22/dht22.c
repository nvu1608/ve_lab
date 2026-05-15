/**
 * @file    dht22.c
 * @brief   DHT22 Pure Logic Simulation implementation.
 */

#include "dht22.h"
#include <string.h>

/* Public functions --------------------------------------------------------*/

void dht22_reset(dht22_t *dev)
{
    if (dev == NULL) return;
    dht22_set(dev, 600, 255); /* Default: 60.0% RH, 25.5C */
    dev->current_bit = 0;
}

void dht22_set(dht22_t *dev, uint16_t humidity_x10, int16_t temp_x10)
{
    if (dev == NULL) return;
    
    dev->humidity    = humidity_x10;
    dev->temperature = temp_x10;
    
    /* DHT22 40-bit frame format:
     * Byte 0: Humidity High Byte
     * Byte 1: Humidity Low Byte
     * Byte 2: Temperature High Byte
     * Byte 3: Temperature Low Byte
     * Byte 4: Checksum (Sum of Bytes 0-3)
     */
    dev->frame[0] = (uint8_t)((humidity_x10 >> 8) & 0xFFu);
    dev->frame[1] = (uint8_t)(humidity_x10 & 0xFFu);
    
    uint16_t t_raw = (temp_x10 < 0) ? (uint16_t)((uint16_t)(-temp_x10) | 0x8000u) : (uint16_t)temp_x10;
    dev->frame[2] = (uint8_t)((t_raw >> 8) & 0xFFu);
    dev->frame[3] = (uint8_t)(t_raw & 0xFFu);
    
    dev->frame[4] = (uint8_t)((dev->frame[0] + dev->frame[1] + dev->frame[2] + dev->frame[3]) & 0xFFu);
}

uint8_t dht22_tx(dht22_t *dev)
{
    if (dev == NULL || dev->current_bit >= 40) return 0u;
    
    uint8_t byte_idx = dev->current_bit / 8;
    uint8_t bit_pos  = 7 - (dev->current_bit % 8);
    uint8_t bit      = (dev->frame[byte_idx] >> bit_pos) & 0x01u;
    
    dev->current_bit++;
    return bit;
}

void dht22_reset_comm(dht22_t *dev)
{
    if (dev == NULL) return;
    dev->current_bit = 0;
}
