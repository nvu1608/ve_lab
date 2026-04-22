#include "stm32f10x.h"

#define DS1307_ADDR     0x68
#define I2C_TIMEOUT     10000
#define SCAN_TIMEOUT    100000UL

void     I2C_Config(void);
uint8_t  I2C_Start(void);
uint8_t  I2C_SendAddr_Write(uint8_t addr);
uint8_t  I2C_SendAddr_Read(uint8_t addr);
uint8_t  I2C_WriteByte(uint8_t data);
uint8_t  I2C_ReadByte_ACK(void);
uint8_t  I2C_ReadByte_NACK(void);
void     I2C_Stop(void);

uint8_t  I2C_ScanUntilFound(uint8_t target);
void     DS1307_SetTime(uint8_t h, uint8_t m, uint8_t s);
uint8_t  DS1307_ReadTime(uint8_t *h, uint8_t *m, uint8_t *s);

void     Uart_Config(void);
void     SendChar(char c);
void     SendString(char *s);
void     SendNumber(uint8_t n);
void     SendHex(uint8_t val);
void     delay_ms(uint16_t ms);

uint8_t  BCD_Encode(uint8_t val);
uint8_t  BCD_Decode(uint8_t bcd);


/* =======================================================
 *  MAIN
 * ======================================================= */
int main(void)
{
    I2C_Config();
    Uart_Config();

    SendString("=== STM32 DS1307 Debug ===\n");

    /* --- Scan den khi gap DS1307, neu khong thay thi treo --- */
    if (!I2C_ScanUntilFound(DS1307_ADDR))
    {
        SendString("ERROR: DS1307 not found!\n");
        while (1);
    }

    SendString("DS1307 found. Starting...\n");

    /* --- Set time 15:22:23 --- */
    DS1307_SetTime(15, 22, 23);

    /* --- Vong lap doc va in thoi gian --- */
    uint8_t h, m, s;
    while (1)
    {
        if (DS1307_ReadTime(&h, &m, &s))
        {
            SendString("Time: ");
            SendNumber(h); SendChar(':');
            SendNumber(m); SendChar(':');
            SendNumber(s); SendChar('\n');
        }
        else
        {
            SendString("Read error!\n");
        }
        delay_ms(1000);
    }
}

/* =======================================================
 *  I2C SCAN — quet tu 0x01, gap ACK o dia chi target thi
 *  DUNG LUON, tra ve 1. Neu het 0x7F khong thay tra ve 0.
 *
 *  Moi dia chi:
 *    START → addr+W → cho ACK/NACK/timeout → STOP → next
 *  Neu ACK trung voi target → in "Found" → thoat ngay
 *  Neu ACK nhung khong phai target → in "Found (other)" → tiep tuc
 * ======================================================= */
uint8_t I2C_ScanUntilFound(uint8_t target)
{
    uint8_t  addr;
    uint8_t  ack_ok;
    volatile uint32_t t;

    SendString("Scanning I2C bus...\n");

    for (addr = 0x01; addr <= 0x7F; addr++)
    {
        ack_ok = 0;

        /* Cho bus ranh; neu treo thi reset */
        t = SCAN_TIMEOUT;
        while (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY))
        {
            if (--t == 0)
            {
                I2C_SoftwareResetCmd(I2C1, ENABLE);
                I2C_SoftwareResetCmd(I2C1, DISABLE);
                I2C_Cmd(I2C1, ENABLE);
                break;
            }
        }

        /* Tao START */
        I2C_GenerateSTART(I2C1, ENABLE);
        t = SCAN_TIMEOUT;
        while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT))
        {
            if (--t == 0) break;
        }

        /* Gui dia chi neu START ok */
        if (t > 0)
        {
            I2C_Send7bitAddress(I2C1, addr << 1, I2C_Direction_Transmitter);

            t = SCAN_TIMEOUT;
            while (t > 0)
            {
                if (I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
                {
                    ack_ok = 1;
                    break;
                }
                if (I2C_GetFlagStatus(I2C1, I2C_FLAG_AF))
                {
                    I2C_ClearFlag(I2C1, I2C_FLAG_AF);
                    break;
                }
                --t;
            }
        }

        /* Luon STOP de giai phong bus */
        I2C_GenerateSTOP(I2C1, ENABLE);

        /* Cho STOP hoan thanh */
        t = SCAN_TIMEOUT;
        while (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY))
        {
            if (--t == 0)
            {
                I2C_SoftwareResetCmd(I2C1, ENABLE);
                I2C_SoftwareResetCmd(I2C1, DISABLE);
                I2C_Cmd(I2C1, ENABLE);
                break;
            }
        }

        /* Xu ly ket qua cua dia chi nay */
        if (ack_ok)
        {
            SendString("Found: 0x");
            SendHex(addr);
            SendChar('\n');

            if (addr == target)
            {
                /* Dung luon — day chinh la thiet bi can tim */
                return 1;
            }
            /* Neu ACK nhung khong phai target: in va scan tiep */
        }
    }

    /* Het dia chi ma khong thay target */
    return 0;
}

/* =======================================================
 *  I2C CONFIG
 * ======================================================= */
void I2C_Config(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1,  ENABLE);

    GPIO_InitTypeDef gpio;
    gpio.GPIO_Mode  = GPIO_Mode_AF_OD;
    gpio.GPIO_Pin   = GPIO_Pin_6 | GPIO_Pin_7;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio);

    I2C_SoftwareResetCmd(I2C1, ENABLE);
    I2C_SoftwareResetCmd(I2C1, DISABLE);

    I2C_InitTypeDef i2c;
    i2c.I2C_Ack                 = I2C_Ack_Enable;
    i2c.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    i2c.I2C_ClockSpeed          = 100000; // test false 400kHz 
    i2c.I2C_DutyCycle           = I2C_DutyCycle_2;
    i2c.I2C_Mode                = I2C_Mode_I2C;
    i2c.I2C_OwnAddress1         = 0x00;
    I2C_Init(I2C1, &i2c);
    I2C_Cmd(I2C1, ENABLE);
}

/* =======================================================
 *  I2C PRIMITIVES
 * ======================================================= */
uint8_t I2C_Start(void)
{
    volatile uint32_t t = I2C_TIMEOUT;
    while (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY))
        if (--t == 0) return 0;

    I2C_GenerateSTART(I2C1, ENABLE);

    t = I2C_TIMEOUT;
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT))
        if (--t == 0) return 0;

    return 1;
}

uint8_t I2C_SendAddr_Write(uint8_t addr)
{
    I2C_Send7bitAddress(I2C1, addr << 1, I2C_Direction_Transmitter);

    volatile uint32_t t = I2C_TIMEOUT;
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
        if (--t == 0) return 0;

    return 1;
}

uint8_t I2C_SendAddr_Read(uint8_t addr)
{
    I2C_Send7bitAddress(I2C1, addr << 1, I2C_Direction_Receiver);

    volatile uint32_t t = I2C_TIMEOUT;
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED))
        if (--t == 0) return 0;

    return 1;
}

uint8_t I2C_WriteByte(uint8_t data)
{
    I2C_SendData(I2C1, data);

    volatile uint32_t t = I2C_TIMEOUT;
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED))
        if (--t == 0) return 0;

    return 1;
}

uint8_t I2C_ReadByte_ACK(void)
{
    I2C_AcknowledgeConfig(I2C1, ENABLE);

    volatile uint32_t t = I2C_TIMEOUT;
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_RECEIVED))
        if (--t == 0) return 0xFF;

    return I2C_ReceiveData(I2C1);
}

uint8_t I2C_ReadByte_NACK(void)
{
    I2C_AcknowledgeConfig(I2C1, DISABLE);
    I2C_GenerateSTOP(I2C1, ENABLE);

    volatile uint32_t t = I2C_TIMEOUT;
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_RECEIVED))
        if (--t == 0) return 0xFF;

    return I2C_ReceiveData(I2C1);
}

void I2C_Stop(void)
{
    I2C_GenerateSTOP(I2C1, ENABLE);
}

/* =======================================================
 *  BCD
 * ======================================================= */
uint8_t BCD_Encode(uint8_t val)
{
    return ((val / 10) << 4) | (val % 10);
}

uint8_t BCD_Decode(uint8_t bcd)
{
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

/* =======================================================
 *  DS1307 SET TIME
 *  Frame: [START][0x68+W][0x00][s][m][h][STOP]
 * ======================================================= */
void DS1307_SetTime(uint8_t h, uint8_t m, uint8_t s)
{
    if (!I2C_Start())
    {
        SendString("SetTime: START fail\n");
        return;
    }
    if (!I2C_SendAddr_Write(DS1307_ADDR))
    {
        I2C_Stop();
        SendString("SetTime: ADDR fail\n");
        return;
    }

    I2C_WriteByte(0x00);                   /* Register pointer: 0x00 (giay) */
    I2C_WriteByte(BCD_Encode(s) & 0x7F);  /* Giay — bit7=0: bat dao dong    */
    I2C_WriteByte(BCD_Encode(m));          /* Phut                           */
    I2C_WriteByte(BCD_Encode(h));          /* Gio — bit6=0: 24h mode         */
    I2C_Stop();

    SendString("Set time: ");
    SendNumber(h); SendChar(':');
    SendNumber(m); SendChar(':');
    SendNumber(s); SendChar('\n');
}

/* =======================================================
 *  DS1307 READ TIME
 *  Frame 1: [START][0x68+W][0x00][STOP]       <- dat con tro
 *  Frame 2: [START][0x68+R][s+ACK][m+ACK][h+NACK+STOP]
 * ======================================================= */
uint8_t DS1307_ReadTime(uint8_t *h, uint8_t *m, uint8_t *s)
{
    /* Frame 1: dat con tro ve register 0x00 */
    if (!I2C_Start())                     return 0;
    if (!I2C_SendAddr_Write(DS1307_ADDR)) { I2C_Stop(); return 0; }
    if (!I2C_WriteByte(0x00))             { I2C_Stop(); return 0; }
    I2C_Stop();

    /* Frame 2: doc 3 byte */
    if (!I2C_Start())                    return 0;
    if (!I2C_SendAddr_Read(DS1307_ADDR)) { I2C_Stop(); return 0; }

    *s = I2C_ReadByte_ACK();   /* Byte 1: giay  + ACK          */
    *m = I2C_ReadByte_ACK();   /* Byte 2: phut  + ACK          */
    *h = I2C_ReadByte_NACK();  /* Byte 3: gio   + NACK + STOP  */

    *s = BCD_Decode(*s & 0x7F);  /* mask bit7 CH               */
    *m = BCD_Decode(*m);
    *h = BCD_Decode(*h & 0x3F);  /* mask bit6 12/24h           */

    return 1;
}

/* =======================================================
 *  UART
 * ======================================================= */
void Uart_Config(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitTypeDef gpio;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    gpio.GPIO_Pin   = GPIO_Pin_9;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    gpio.GPIO_Pin  = GPIO_Pin_10;
    GPIO_Init(GPIOA, &gpio);

    USART_InitTypeDef uart;
    uart.USART_BaudRate            = 115200;
    uart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    uart.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;
    uart.USART_Parity              = USART_Parity_No;
    uart.USART_StopBits            = USART_StopBits_1;
    uart.USART_WordLength          = USART_WordLength_8b;
    USART_Init(USART1, &uart);
    USART_Cmd(USART1, ENABLE);
}

void SendChar(char c)
{
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    USART_SendData(USART1, (uint8_t)c);
}

void SendString(char *s)
{
    while (*s) SendChar(*s++);
}

void SendNumber(uint8_t n)
{
    char buf[3];
    buf[0] = '0' + n / 10;
    buf[1] = '0' + n % 10;
    buf[2] = '\0';
    SendString(buf);
}

void SendHex(uint8_t val)
{
    const char hex[] = "0123456789ABCDEF";
    SendChar(hex[val >> 4]);
    SendChar(hex[val & 0x0F]);
}

/* =======================================================
 *  DELAY (~1ms @ 72MHz)
 * ======================================================= */
void delay_ms(uint16_t ms)
{
    volatile uint32_t i, j;
    for (i = 0; i < ms; i++)
        for (j = 0; j < 7200; j++);
}