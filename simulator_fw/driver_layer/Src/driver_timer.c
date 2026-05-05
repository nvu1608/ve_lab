#include "driver_timer.h"
#include <string.h>

/* ================================================================
 *  PRIVATE MAPPING DATA
 * ================================================================ */

typedef struct
{
    DMA_Channel_TypeDef *ch;
    uint32_t flag_tc; /* Transfer Complete flag */
    uint32_t flag_gl; /* Global flag to clear */
    IRQn_Type irqn;
} prv_dma_map_t;

static const prv_dma_map_t dma_map[4][4] = {
    /* TIM1 */
    {
        {DMA1_Channel2, DMA1_FLAG_TC2, DMA1_FLAG_GL2, DMA1_Channel2_IRQn}, /* CH1 */
        {DMA1_Channel3, DMA1_FLAG_TC3, DMA1_FLAG_GL3, DMA1_Channel3_IRQn}, /* CH2 */
        {DMA1_Channel6, DMA1_FLAG_TC6, DMA1_FLAG_GL6, DMA1_Channel6_IRQn}, /* CH3 */
        {DMA1_Channel4, DMA1_FLAG_TC4, DMA1_FLAG_GL4, DMA1_Channel4_IRQn}, /* CH4 */
    },
    /* TIM2 */
    {
        {DMA1_Channel5, DMA1_FLAG_TC5, DMA1_FLAG_GL5, DMA1_Channel5_IRQn}, /* CH1 */
        {DMA1_Channel7, DMA1_FLAG_TC7, DMA1_FLAG_GL7, DMA1_Channel7_IRQn}, /* CH2 */
        {DMA1_Channel1, DMA1_FLAG_TC1, DMA1_FLAG_GL1, DMA1_Channel1_IRQn}, /* CH3 */
        {DMA1_Channel7, DMA1_FLAG_TC7, DMA1_FLAG_GL7, DMA1_Channel7_IRQn}, /* CH4 */
    },
    /* TIM3 */
    {
        {DMA1_Channel6, DMA1_FLAG_TC6, DMA1_FLAG_GL6, DMA1_Channel6_IRQn}, /* CH1 */
        {DMA1_Channel7, DMA1_FLAG_TC7, DMA1_FLAG_GL7, DMA1_Channel7_IRQn}, /* CH2 */
        {DMA1_Channel2, DMA1_FLAG_TC2, DMA1_FLAG_GL2, DMA1_Channel2_IRQn}, /* CH3 */
        {DMA1_Channel3, DMA1_FLAG_TC3, DMA1_FLAG_GL3, DMA1_Channel3_IRQn}, /* CH4 */
    },
    /* TIM4 */
    {
        {DMA1_Channel1, DMA1_FLAG_TC1, DMA1_FLAG_GL1, DMA1_Channel1_IRQn},
        {DMA1_Channel4, DMA1_FLAG_TC4, DMA1_FLAG_GL4, DMA1_Channel4_IRQn},
        {DMA1_Channel5, DMA1_FLAG_TC5, DMA1_FLAG_GL5, DMA1_Channel5_IRQn},
        {NULL, 0, 0, (IRQn_Type)0}, /* TIM4_CH4 no DMA */
    },
};

/* ================================================================
 *  PRIVATE HELPERS
 * ================================================================ */

static int prv_get_tim_idx(TIM_TypeDef *instance)
{
    if (instance == TIM1) return 0;
    if (instance == TIM2) return 1;
    if (instance == TIM3) return 2;
    if (instance == TIM4) return 3;
    return -1;
}

static IRQn_Type prv_get_tim_cc_irqn(TIM_TypeDef *instance)
{
    if (instance == TIM1) return TIM1_CC_IRQn;
    if (instance == TIM2) return TIM2_IRQn;
    if (instance == TIM3) return TIM3_IRQn;
    if (instance == TIM4) return TIM4_IRQn;
    return TIM2_IRQn;
}

static IRQn_Type prv_get_tim_up_irqn(TIM_TypeDef *instance)
{
    if (instance == TIM1) return TIM1_UP_IRQn;
    if (instance == TIM2) return TIM2_IRQn;
    if (instance == TIM3) return TIM3_IRQn;
    if (instance == TIM4) return TIM4_IRQn;
    return TIM2_IRQn;
}

static uint16_t prv_get_it_flag(timer_channel_t ch)
{
    static const uint16_t flags[] = {TIM_IT_CC1, TIM_IT_CC2, TIM_IT_CC3, TIM_IT_CC4};
    return flags[ch];
}

static uint16_t prv_get_dma_src(timer_channel_t ch)
{
    static const uint16_t srcs[] = {TIM_DMA_CC1, TIM_DMA_CC2, TIM_DMA_CC3, TIM_DMA_CC4};
    return srcs[ch];
}

static uint32_t prv_get_ccr_addr(TIM_TypeDef *instance, timer_channel_t ch)
{
    switch (ch) {
        case TIMER_CH1: return (uint32_t)&instance->CCR1;
        case TIMER_CH2: return (uint32_t)&instance->CCR2;
        case TIMER_CH3: return (uint32_t)&instance->CCR3;
        case TIMER_CH4: return (uint32_t)&instance->CCR4;
        default: return 0;
    }
}

static void prv_oc_init(TIM_TypeDef *instance, timer_channel_t ch, TIM_OCInitTypeDef *oc)
{
    switch (ch) {
        case TIMER_CH1: TIM_OC1Init(instance, oc); TIM_OC1PreloadConfig(instance, TIM_OCPreload_Enable); break;
        case TIMER_CH2: TIM_OC2Init(instance, oc); TIM_OC2PreloadConfig(instance, TIM_OCPreload_Enable); break;
        case TIMER_CH3: TIM_OC3Init(instance, oc); TIM_OC3PreloadConfig(instance, TIM_OCPreload_Enable); break;
        case TIMER_CH4: TIM_OC4Init(instance, oc); TIM_OC4PreloadConfig(instance, TIM_OCPreload_Enable); break;
    }
}

static driver_status_t prv_dma_open(timer_t *dev, timer_channel_t ch, uint16_t *buf, uint16_t len, uint32_t dir)
{
    int tidx = prv_get_tim_idx(dev->instance);
    if (tidx < 0) return DRIVER_ERR_INVALID_ARG;
    
    const prv_dma_map_t *m = &dma_map[tidx][ch];

    if (!m->ch) return DRIVER_ERR_NOT_SUPPORTED;

    /* Note: RCC enable for DMA should ideally be in project layer, 
       but for DMA common resource it's often kept in driver for simplicity. 
       Following standard strictly, it should be external. 
       We'll keep it here for now as DMA1 is a singleton service. */
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    DMA_InitTypeDef dma;
    DMA_DeInit(m->ch);
    dma.DMA_PeripheralBaseAddr = prv_get_ccr_addr(dev->instance, ch);
    dma.DMA_MemoryBaseAddr = (uint32_t)buf;
    dma.DMA_DIR = dir;
    dma.DMA_BufferSize = len;
    dma.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    dma.DMA_MemoryInc = DMA_MemoryInc_Enable;
    dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    dma.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    dma.DMA_Mode = DMA_Mode_Circular;
    dma.DMA_Priority = DMA_Priority_High;
    dma.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(m->ch, &dma);

    DMA_ITConfig(m->ch, DMA_IT_TC, ENABLE);
    NVIC_EnableIRQ(m->irqn);

    TIM_DMACmd(dev->instance, prv_get_dma_src(ch), ENABLE);
    DMA_Cmd(m->ch, ENABLE);

    dev->channels[ch].dma_channel = m->ch;
    return DRIVER_OK;
}

/* ================================================================
 *  PUBLIC API
 * ================================================================ */

driver_status_t timer_init(timer_t *dev, TIM_TypeDef *instance)
{
    if (!dev || !instance) return DRIVER_ERR_INVALID_ARG;
    memset(dev, 0, sizeof(*dev));
    dev->instance = instance;
    return DRIVER_OK;
}

driver_status_t timer_base_init(timer_t *dev, const timer_base_cfg_t *cfg)
{
    if (!dev || !cfg || (cfg->prescaler == 0)) return DRIVER_ERR_INVALID_ARG;

    TIM_TimeBaseInitTypeDef tb;
    TIM_TimeBaseStructInit(&tb);
    tb.TIM_Prescaler = cfg->prescaler - 1;
    tb.TIM_Period = cfg->period - 1;
    tb.TIM_ClockDivision = TIM_CKD_DIV1;
    tb.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(dev->instance, &tb);

    dev->channels[0].mode = TIMER_MODE_BASE;
    dev->channels[0].callback = cfg->callback;
    dev->channels[0].ctx = cfg->ctx;

    TIM_ITConfig(dev->instance, TIM_IT_Update, ENABLE);
    return DRIVER_OK;
}

driver_status_t timer_pwm_init(timer_t *dev, timer_channel_t ch, const timer_pwm_cfg_t *cfg)
{
    if (!dev || !cfg || (cfg->prescaler == 0)) return DRIVER_ERR_INVALID_ARG;

    TIM_TimeBaseInitTypeDef tb;
    TIM_TimeBaseStructInit(&tb);
    tb.TIM_Prescaler = cfg->prescaler - 1;
    tb.TIM_Period = cfg->period - 1;
    TIM_TimeBaseInit(dev->instance, &tb);

    TIM_OCInitTypeDef oc;
    TIM_OCStructInit(&oc);
    oc.TIM_OCMode = TIM_OCMode_PWM1;
    oc.TIM_OutputState = TIM_OutputState_Enable;
    oc.TIM_Pulse = cfg->pulse;
    oc.TIM_OCPolarity = cfg->polarity;
    prv_oc_init(dev->instance, ch, &oc);
    TIM_ARRPreloadConfig(dev->instance, ENABLE);

    if (dev->instance == TIM1) TIM_CtrlPWMOutputs(dev->instance, ENABLE);

    dev->channels[ch].mode = TIMER_MODE_PWM;
    dev->channels[ch].dma_callback = cfg->dma_callback;
    dev->channels[ch].dma_ctx = cfg->dma_ctx;

    if (cfg->dma_enable) {
        return prv_dma_open(dev, ch, cfg->dma_buf, cfg->dma_len, DMA_DIR_PeripheralDST);
    }

    return DRIVER_OK;
}

driver_status_t timer_ic_init(timer_t *dev, timer_channel_t ch, const timer_ic_cfg_t *cfg)
{
    if (!dev || !cfg || (cfg->prescaler == 0)) return DRIVER_ERR_INVALID_ARG;

    TIM_TimeBaseInitTypeDef tb;
    TIM_TimeBaseStructInit(&tb);
    tb.TIM_Prescaler = cfg->prescaler - 1;
    tb.TIM_Period = cfg->period - 1;
    TIM_TimeBaseInit(dev->instance, &tb);

    TIM_ICInitTypeDef ic;
    ic.TIM_Channel = (ch == TIMER_CH1) ? TIM_Channel_1 : (ch == TIMER_CH2) ? TIM_Channel_2 : (ch == TIMER_CH3) ? TIM_Channel_3 : TIM_Channel_4;
    ic.TIM_ICPolarity = (uint16_t)cfg->polarity;
    ic.TIM_ICSelection = TIM_ICSelection_DirectTI;
    ic.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    ic.TIM_ICFilter = cfg->filter;
    TIM_ICInit(dev->instance, &ic);

    dev->channels[ch].mode = TIMER_MODE_IC;
    dev->channels[ch].callback = cfg->callback;
    dev->channels[ch].ctx = cfg->ctx;
    dev->channels[ch].dma_callback = cfg->dma_callback;
    dev->channels[ch].dma_ctx = cfg->dma_ctx;

    if (cfg->dma_enable) {
        return prv_dma_open(dev, ch, cfg->dma_buf, cfg->dma_len, DMA_DIR_PeripheralSRC);
    } else {
        TIM_ITConfig(dev->instance, prv_get_it_flag(ch), ENABLE);
    }

    return DRIVER_OK;
}

driver_status_t timer_oc_init(timer_t *dev, timer_channel_t ch, const timer_oc_cfg_t *cfg)
{
    if (!dev || !cfg || (cfg->prescaler == 0)) return DRIVER_ERR_INVALID_ARG;

    TIM_TimeBaseInitTypeDef tb;
    TIM_TimeBaseStructInit(&tb);
    tb.TIM_Prescaler = cfg->prescaler - 1;
    tb.TIM_Period = cfg->period - 1;
    TIM_TimeBaseInit(dev->instance, &tb);

    TIM_OCInitTypeDef oc;
    TIM_OCStructInit(&oc);
    oc.TIM_OCMode = (uint16_t)cfg->oc_mode;
    oc.TIM_OutputState = TIM_OutputState_Enable;
    oc.TIM_Pulse = cfg->pulse;
    oc.TIM_OCPolarity = cfg->polarity;
    prv_oc_init(dev->instance, ch, &oc);
    TIM_ARRPreloadConfig(dev->instance, ENABLE);

    if (dev->instance == TIM1) TIM_CtrlPWMOutputs(dev->instance, ENABLE);

    dev->channels[ch].mode = TIMER_MODE_OC;
    dev->channels[ch].callback = cfg->callback;
    dev->channels[ch].ctx = cfg->ctx;
    dev->channels[ch].dma_callback = cfg->dma_callback;
    dev->channels[ch].dma_ctx = cfg->dma_ctx;

    if (cfg->dma_enable) {
        return prv_dma_open(dev, ch, cfg->dma_buf, cfg->dma_len, DMA_DIR_PeripheralDST);
    } else {
        TIM_ITConfig(dev->instance, prv_get_it_flag(ch), ENABLE);
    }

    return DRIVER_OK;
}

driver_status_t timer_start(timer_t *dev, timer_channel_t ch)
{
    if (!dev) return DRIVER_ERR_INVALID_ARG;
    
    if (dev->channels[ch].mode == TIMER_MODE_BASE) {
        NVIC_EnableIRQ(prv_get_tim_up_irqn(dev->instance));
    } else {
        NVIC_EnableIRQ(prv_get_tim_cc_irqn(dev->instance));
    }

    TIM_Cmd(dev->instance, ENABLE);
    return DRIVER_OK;
}

driver_status_t timer_stop(timer_t *dev, timer_channel_t ch)
{
    if (!dev) return DRIVER_ERR_INVALID_ARG;
    
    if (dev->channels[ch].dma_channel) {
        DMA_Cmd(dev->channels[ch].dma_channel, DISABLE);
    }
    
    TIM_ITConfig(dev->instance, prv_get_it_flag(ch) | TIM_IT_Update, DISABLE);
    
    uint8_t still_used = 0;
    for (int i = 0; i < 4; i++) {
        if (i != (int)ch && dev->channels[i].mode != TIMER_MODE_NONE) {
            still_used = 1;
            break;
        }
    }
    
    if (!still_used) {
        TIM_Cmd(dev->instance, DISABLE);
    }
    
    dev->channels[ch].mode = TIMER_MODE_NONE;
    return DRIVER_OK;
}

driver_status_t timer_read(timer_t *dev, timer_channel_t ch, uint32_t *val)
{
    if (!dev || !val) return DRIVER_ERR_INVALID_ARG;
    
    switch (dev->channels[ch].mode) {
        case TIMER_MODE_BASE: *val = TIM_GetCounter(dev->instance); break;
        case TIMER_MODE_IC:
            switch (ch) {
                case TIMER_CH1: *val = TIM_GetCapture1(dev->instance); break;
                case TIMER_CH2: *val = TIM_GetCapture2(dev->instance); break;
                case TIMER_CH3: *val = TIM_GetCapture3(dev->instance); break;
                case TIMER_CH4: *val = TIM_GetCapture4(dev->instance); break;
            }
            break;
        default: return DRIVER_ERR_NOT_SUPPORTED;
    }
    return DRIVER_OK;
}

driver_status_t timer_write(timer_t *dev, timer_channel_t ch, uint16_t val)
{
    if (!dev) return DRIVER_ERR_INVALID_ARG;
    
    switch (ch) {
        case TIMER_CH1: TIM_SetCompare1(dev->instance, val); break;
        case TIMER_CH2: TIM_SetCompare2(dev->instance, val); break;
        case TIMER_CH3: TIM_SetCompare3(dev->instance, val); break;
        case TIMER_CH4: TIM_SetCompare4(dev->instance, val); break;
    }
    return DRIVER_OK;
}

void timer_irq_handler(timer_t *dev)
{
    if (!dev) return;

    if (TIM_GetITStatus(dev->instance, TIM_IT_Update) == SET) {
        TIM_ClearITPendingBit(dev->instance, TIM_IT_Update);
        if (dev->channels[0].mode == TIMER_MODE_BASE && dev->channels[0].callback) {
            dev->channels[0].callback(dev->channels[0].ctx, TIMER_CH1, TIM_GetCounter(dev->instance));
        }
    }

    for (int i = 0; i < 4; i++) {
        uint16_t flag = prv_get_it_flag((timer_channel_t)i);
        if (TIM_GetITStatus(dev->instance, flag) == SET) {
            TIM_ClearITPendingBit(dev->instance, flag);
            if (dev->channels[i].callback) {
                uint32_t val = 0;
                timer_read(dev, (timer_channel_t)i, &val);
                dev->channels[i].callback(dev->channels[i].ctx, (timer_channel_t)i, val);
            }
        }
    }
}

void timer_dma_irq_handler(timer_t *dev, timer_channel_t ch)
{
    if (!dev) return;
    
    int tidx = prv_get_tim_idx(dev->instance);
    if (tidx < 0) return;
    const prv_dma_map_t *m = &dma_map[tidx][ch];
    
    if (DMA_GetFlagStatus(m->flag_tc) == SET) {
        DMA_ClearFlag(m->flag_gl);
        if (dev->channels[ch].dma_callback) {
            dev->channels[ch].dma_callback(dev->channels[ch].dma_ctx, ch);
        }
    }
}
