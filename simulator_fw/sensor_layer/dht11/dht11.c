/**
 * @file    dht11.c
 * @brief   DHT11 Pure Logic Simulation implementation.
 */

#include "dht11.h"
#include <string.h>

/* Public functions --------------------------------------------------------*/

void dht11_reset(dht11_t *dev)
{
    if (dev == NULL) return;
    dht11_set(dev, 60, 25); /* Default: 60% RH, 25C */
    dev->current_bit = 0;
}

void dht11_set(dht11_t *dev, uint8_t humidity, uint8_t temperature)
{
    if (dev == NULL) return;
    
    dev->humidity    = humidity;
    dev->temperature = temperature;
    
    /* Build 40-bit frame: 8-bit Hum Int, 8-bit Hum Dec, 8-bit Temp Int, 8-bit Temp Dec, 8-bit Checksum */
    dev->frame[0] = humidity;
    dev->frame[1] = 0;      /* DHT11 decimal parts are 0 */
    dev->frame[2] = temperature;
    dev->frame[3] = 0;
    dev->frame[4] = (uint8_t)(dev->frame[0] + dev->frame[1] + dev->frame[2] + dev->frame[3]);
}

uint8_t dht11_tx(dht11_t *dev)
{
    if (dev == NULL || dev->current_bit >= 40) return 0u;
    
    uint8_t byte_idx = dev->current_bit / 8;
    uint8_t bit_pos  = 7 - (dev->current_bit % 8);
    uint8_t bit      = (dev->frame[byte_idx] >> bit_pos) & 0x01;
    
    dev->current_bit++;
    return bit;
}

void dht11_reset_comm(dht11_t *dev)
{
    if (dev == NULL) return;
    dev->current_bit = 0;
}
