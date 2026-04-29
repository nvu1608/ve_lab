# Peripheral Drivers Review & Roadmap

Tai lieu nay danh gia trang thai cua cac driver ngoai vi trong Simulator Firmware va de xuat huong phat trien tiep theo.

## 1. Trang thai hien tai (Status Report)

Toan bo cac driver cot loi da duoc refactor theo chuan **OOP-based in C** va **Hardware Decoupling**.

| Ngoai vi | Trang thai | Ghi chu |
| :--- | :--- | :--- |
| **RCC** | Hoan thanh | Ho tro bat/tat clock cho APB1, APB2, AHB. |
| **GPIO** | Hoan thanh | Ho tro vao/ra, AF, va EXTI interrupt. |
| **UART** | Hoan thanh | Chay che do ngat (Interrupt-driven), ho tro callback. |
| **I2C Slave** | Hoan thanh | Ho tro event callback (Addr match, RX, TX done). |
| **Timer** | Hoan thanh | Ho tro Base, PWM, IC, OC va DMA. Dung callback context. |
| **DMA** | Hoan thanh | Tich hop ben trong cac driver UART, Timer, I2C. |

## 2. Danh gia Kien truc (Architecture Review)

### Ưu điểm:
- **Tinh module hoa cao**: Cac sensor layer (DHT11, DS1307) khong con truy cap register, de dang thay doi hardware ma khong sua logic sensor.
- **Quan ly tai nguyen tot**: Su dung singleton pattern ket hop voi object context giup tranh race-condition va leak memory/DMA.

### Nhược điểm / Han che:
- **Latency**: Viec qua nhieu lop callback co the tang latency mot chut, nhung khong dang ke voi cac sensor toc do thap.

## 3. Roadmap (Huong phat trien tiep theo)

1. **ADC Driver**: Can thiet de gia lap cac cam bien Analog.
2. **SPI Slave Driver**: De gia lap cac thiet bi SPI (VD: Flash memory, RFID).

---
*Cap nhat lan cuoi: 2026-04-30*
