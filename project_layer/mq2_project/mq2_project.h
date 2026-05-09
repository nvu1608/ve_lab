/**
 * @file    mq2_project.h
 * @brief   MQ-2 simulation project interface.
 * @details Provides the project-level initialization API for MQ-2 simulation.
 */

#ifndef MQ2_PROJECT_H
#define MQ2_PROJECT_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Public API --------------------------------------------------------------*/

/**
 * @brief Initialize MQ-2 simulation project.
 * @details Configures hardware resources, initializes DAC driver, and starts
 *          the MQ-2 simulator task.
 */
void mq2_project_init(void);

#ifdef __cplusplus
}
#endif

#endif /* MQ2_PROJECT_H */