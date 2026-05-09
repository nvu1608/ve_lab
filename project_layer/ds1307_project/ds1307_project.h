/**
 * @file    ds1307_project.h
 * @brief   DS1307 simulation project interface.
 * @details Provides the project-level initialization API for DS1307 simulation.
 */

#ifndef DS1307_PROJECT_H
#define DS1307_PROJECT_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Public API --------------------------------------------------------------*/

/**
 * @brief Initialize DS1307 simulation project.
 * @details Configures hardware resources, initializes drivers, and starts the
 *          DS1307 simulator task.
 */
void ds1307_project_init(void);

#ifdef __cplusplus
}
#endif

#endif /* DS1307_PROJECT_H */
