#include "driver_gpio.h"
#include <string.h>

static uint8_t gpio_get_port_source(GPIO_TypeDef *port)
{
    if (port == GPIOA)
    {
        return GPIO_PortSourceGPIOA;
    }
    else if (port == GPIOB)
    {
        return GPIO_PortSourceGPIOB;
    }
    else if (port == GPIOC)
    {
        return GPIO_PortSourceGPIOC;
    }
    else if (port == GPIOD)
    {
        return GPIO_PortSourceGPIOD;
    }
    else if (port == GPIOE)
    {
        return GPIO_PortSourceGPIOE;
    }

    return GPIO_PortSourceGPIOA;
}

static uint8_t gpio_get_pin_source(uint16_t pin)
{
    uint8_t source = 0u;

    while (((pin >> source) & 0x01u) == 0u)
    {
        source++;
    }

    return source;
}

static uint32_t gpio_get_exti_line(uint16_t pin)
{
    return (uint32_t)pin;
}

static IRQn_Type gpio_get_exti_irq(uint16_t pin)
{
    if (pin == GPIO_Pin_0)
    {
        return EXTI0_IRQn;
    }
    else if (pin == GPIO_Pin_1)
    {
        return EXTI1_IRQn;
    }
    else if (pin == GPIO_Pin_2)
    {
        return EXTI2_IRQn;
    }
    else if (pin == GPIO_Pin_3)
    {
        return EXTI3_IRQn;
    }
    else if (pin == GPIO_Pin_4)
    {
        return EXTI4_IRQn;
    }
    else if ((pin & (GPIO_Pin_5 |
                    GPIO_Pin_6 |
                    GPIO_Pin_7 |
                    GPIO_Pin_8 |
                    GPIO_Pin_9)) != 0u)
    {
        return EXTI9_5_IRQn;
    }

    return EXTI15_10_IRQn;
}

static void gpio_exti_start(gpio_t *dev)
{
    EXTI_InitTypeDef exti;
    NVIC_InitTypeDef nvic;

    if ((dev == NULL) || (dev->port == NULL))
    {
        return;
    }

    GPIO_EXTILineConfig(gpio_get_port_source(dev->port),
                        gpio_get_pin_source(dev->pin));

    exti.EXTI_Line = gpio_get_exti_line(dev->pin);
    exti.EXTI_Mode = EXTI_Mode_Interrupt;
    exti.EXTI_Trigger = dev->exti_trigger;
    exti.EXTI_LineCmd = ENABLE;
    EXTI_Init(&exti);

    nvic.NVIC_IRQChannel = gpio_get_exti_irq(dev->pin);
    nvic.NVIC_IRQChannelPreemptionPriority = dev->exti_preempt_priority;
    nvic.NVIC_IRQChannelSubPriority = dev->exti_sub_priority;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);
}

static void gpio_exti_stop(gpio_t *dev)
{
    EXTI_InitTypeDef exti;

    if (dev == NULL)
    {
        return;
    }

    exti.EXTI_Line = gpio_get_exti_line(dev->pin);
    exti.EXTI_Mode = EXTI_Mode_Interrupt;
    exti.EXTI_Trigger = dev->exti_trigger;
    exti.EXTI_LineCmd = DISABLE;
    EXTI_Init(&exti);

    EXTI_ClearITPendingBit(gpio_get_exti_line(dev->pin));
}

void gpio_init(gpio_t *dev,
               GPIO_TypeDef *port,
               uint16_t pin,
               GPIOMode_TypeDef mode,
               GPIOSpeed_TypeDef speed)
{
    if (dev == NULL)
    {
        return;
    }

    memset(dev, 0, sizeof(*dev));

    dev->port = port;
    dev->pin = pin;
    dev->mode = mode;
    dev->speed = speed;

    dev->exti_state = GPIO_EXTI_DISABLE;
    dev->exti_trigger = EXTI_Trigger_Falling;
    dev->exti_preempt_priority = 0u;
    dev->exti_sub_priority = 0u;

    dev->state = GPIO_STATE_IDLE;
}

void gpio_set_exti(gpio_t *dev,
                   EXTITrigger_TypeDef trigger,
                   uint8_t preempt_priority,
                   uint8_t sub_priority)
{
    if (dev == NULL)
    {
        return;
    }

    dev->exti_state = GPIO_EXTI_ENABLE;
    dev->exti_trigger = trigger;
    dev->exti_preempt_priority = preempt_priority;
    dev->exti_sub_priority = sub_priority;
}

void gpio_start(gpio_t *dev)
{
    GPIO_InitTypeDef gpio_init;

    if ((dev == NULL) || (dev->port == NULL))
    {
        return;
    }

    gpio_init.GPIO_Pin = dev->pin;
    gpio_init.GPIO_Mode = dev->mode;
    gpio_init.GPIO_Speed = dev->speed;

    GPIO_Init(dev->port, &gpio_init);

    if (dev->exti_state == GPIO_EXTI_ENABLE)
    {
        gpio_exti_start(dev);
    }

    dev->state = GPIO_STATE_READY;
}

void gpio_stop(gpio_t *dev)
{
    if ((dev == NULL) || (dev->port == NULL))
    {
        return;
    }

    if (dev->exti_state == GPIO_EXTI_ENABLE)
    {
        gpio_exti_stop(dev);
    }

    GPIO_DeInit(dev->port);

    dev->state = GPIO_STATE_IDLE;
}

uint8_t gpio_read(gpio_t *dev)
{
    if ((dev == NULL) || (dev->port == NULL))
    {
        return 0u;
    }

    return GPIO_ReadInputDataBit(dev->port, dev->pin);
}

void gpio_write(gpio_t *dev, uint8_t level)
{
    if ((dev == NULL) || (dev->port == NULL))
    {
        return;
    }

    if (level != 0u)
    {
        GPIO_SetBits(dev->port, dev->pin);
    }
    else
    {
        GPIO_ResetBits(dev->port, dev->pin);
    }
}

void gpio_set(gpio_t *dev)
{
    if ((dev == NULL) || (dev->port == NULL))
    {
        return;
    }

    GPIO_SetBits(dev->port, dev->pin);
}

void gpio_reset(gpio_t *dev)
{
    if ((dev == NULL) || (dev->port == NULL))
    {
        return;
    }

    GPIO_ResetBits(dev->port, dev->pin);
}

void gpio_toggle(gpio_t *dev)
{
    if ((dev == NULL) || (dev->port == NULL))
    {
        return;
    }

    if (GPIO_ReadOutputDataBit(dev->port, dev->pin) != 0u)
    {
        GPIO_ResetBits(dev->port, dev->pin);
    }
    else
    {
        GPIO_SetBits(dev->port, dev->pin);
    }
}

void gpio_set_exti_callback(gpio_t *dev,
                            gpio_exti_cb_t cb,
                            void *ctx)
{
    if (dev == NULL)
    {
        return;
    }

    dev->exti_cb = cb;
    dev->exti_ctx = ctx;
}

void gpio_exti_handler(gpio_t *dev)
{
    if ((dev == NULL) || (dev->port == NULL))
    {
        return;
    }

    if (EXTI_GetITStatus(gpio_get_exti_line(dev->pin)) != RESET)
    {
        EXTI_ClearITPendingBit(gpio_get_exti_line(dev->pin));

        if (dev->exti_cb != NULL)
        {
            dev->exti_cb(dev->exti_ctx);
        }
    }
}