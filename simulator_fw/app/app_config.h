#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* ============================================================
 * SENSOR ENABLE CONFIGURATION
 * ============================================================ */

#define ENABLE_DS1307 1
#define ENABLE_DHT11  1

/* DS1307 Simulator Configuration */
#define APP_DS1307_I2C_OWN_ADDRESS      0x68
#define APP_DS1307_I2C_CLOCK_SPEED      100000
#define APP_DS1307_TASK_STACK_SIZE      256
#define APP_DS1307_TASK_PRIORITY_OFFSET 1
#define APP_DS1307_TICK_PERIOD_MS       1000

#endif // APP_CONFIG_H
