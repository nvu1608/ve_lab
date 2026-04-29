#ifndef DRIVER_COMMON_H
#define DRIVER_COMMON_H

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{
    DRIVER_OK = 0,
    DRIVER_ERR_INVALID_ARG = -1,
    DRIVER_ERR_TIMEOUT = -2,
    DRIVER_ERR_BUSY = -3,
    DRIVER_ERR_NOT_SUPPORTED = -4,
    DRIVER_ERR_MEM = -5
} driver_status_t;

#ifdef __cplusplus
}
#endif

#endif
