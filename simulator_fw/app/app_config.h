#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* ============================================================
 * SENSOR ENABLE CONFIGURATION
 * ============================================================ */

#ifndef ENABLE_DS1307
#define ENABLE_DS1307 0
#endif

#ifndef ENABLE_DHT11
#define ENABLE_DHT11  0
#endif

/* DS1307 Simulator Configuration */
#define APP_DS1307_I2C_OWN_ADDRESS      0x68
#define APP_DS1307_I2C_CLOCK_SPEED      100000
#define APP_DS1307_TASK_STACK_SIZE      256
#define APP_DS1307_TASK_PRIORITY_OFFSET 1
#define APP_DS1307_TICK_PERIOD_MS       1000

/* DHT11 Simulator Configuration */
#define APP_DHT11_INIT_HUMIDITY         60
#define APP_DHT11_INIT_TEMPERATURE      25
#define APP_DHT11_TASK_STACK_SIZE       128
#define APP_DHT11_TASK_PRIORITY_OFFSET  2

#endif // APP_CONFIG_H
