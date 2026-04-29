# Thay đổi Architecture — Embedded Hardware Autograder

## Vấn đề hiện tại

Các bài lab hiện tại được thiết kế theo từng **ngoại vi đơn lẻ** (GPIO, UART, SPI, I2C...). Mỗi bài có một `simulator.hex` riêng biệt được viết từ đầu. Khi gặp bài **tổng hợp** (ví dụ: đọc DHT11 + DS1307 rồi hiển thị LCD), phải viết lại và compile firmware mới cho Simulator — không tái sử dụng được code cũ.

---

## Giải pháp — Firmware Simulator dạng thư viện 3 lớp

Xây dựng một bộ thư viện cho **STM32 Simulator** gồm 3 lớp:

```
App Layer        → app/app_config.h và app/main.c
                   Chỉ chứa config (#define) và hàm khởi tạo. Không có logic phức tạp.
                   Quyết định sensor nào được compile vào firmware.

Sensor Layer     → sensor/Src/ và sensor/Inc/
                   Mỗi sensor (ví dụ: ds1307.c, dht11.c) là một gói hoàn chỉnh gồm:
                   - Logic mô phỏng (BCD convert, timing...)
                   - RTOS Task độc lập
                   - Khởi tạo phần cứng (Hardware Binding)
                   Sensor layer sẽ tự do gọi API từ Driver Layer.

Driver Layer     → driver/Src/ driver/Inc/
                   Cung cấp API ngoại vi cơ bản (Bare-metal) theo chuẩn OOP:
                   - driver_i2c_slave.c, driver_gpio.c, driver_timer.c...
                   - Thiết kế hướng đối tượng (truyền con trỏ struct `device_t`).
                   - Cơ chế Callback kèm Context (`void *ctx`) để đẩy dữ liệu lên Sensor Layer.
                   Không chứa logic của cảm biến, chỉ lo việc truyền nhận dữ liệu.
```

**App layer không chứa logic** — chỉ là file config `app_config.h` với các `#define`:

```c
// app_config.h — giáo viên chỉ sửa file này cho mỗi bài
#define ENABLE_DS1307
#define ENABLE_DHT11
// #define ENABLE_LCD
```

`main.c` của Simulator dùng các define đó để enable task tương ứng:

```c
int main(void) {
    driver_init();

#ifdef ENABLE_DS1307
    ds1307_config();   // xTaskCreate ds1307_task vào RTOS scheduler
#endif

#ifdef ENABLE_DHT11
    dht11_config();    // xTaskCreate dht11_task vào RTOS scheduler
#endif

    vTaskStartScheduler();  // các task tự chạy từ đây
}
```

Mỗi bài lab chỉ cần thay `app_config.h` → compile → ra `simulator.hex` riêng, **pre-build sẵn** và để vào `assignment/labX/simulator.hex`. Pi chỉ việc flash, không compile gì thêm.

> Bộ thư viện này được giáo viên viết sẵn, sinh viên không cần quan tâm đến phía Simulator.

---

## Phạm vi thay đổi

### Sinh viên
Không thay đổi gì — vẫn chỉ code `main.c` cho **STM32 Student**, push lên GitHub, nhận kết quả Pass/Fail trên tab Actions. Sinh viên không biết có Simulator phía sau, chỉ thấy như đang làm việc với phần cứng thật.

### Giáo viên / Hệ thống
- Firmware Simulator được tái sử dụng qua bộ thư viện 3 lớp
- Thiết kế lại danh sách bài lab và cấu trúc `assignment/`

---

## Lộ trình bài lab

### Giai đoạn 1 — Ngoại vi đơn (giữ nguyên)
Tên theo giao thức, sinh viên học từng peripheral cơ bản.

| Lab | Nội dung |
|-----|----------|
| lab1_gpio | Điều khiển GPIO cơ bản |
| lab2_uart | Giao tiếp UART |
| lab3_timer | Timer / PWM |
| lab4_spi | Giao tiếp SPI |
| lab5_i2c | Giao tiếp I2C |

### Giai đoạn 2 — Sensor / Device (mới)
Tên theo sensor/thiết bị, sinh viên ứng dụng giao thức vào thiết bị thực tế.

| Lab | Nội dung | Giao thức |
|-----|----------|-----------|
| lab6_ds1307 | Đọc/ghi thời gian thực | I2C |
| lab7_dht11 | Đọc nhiệt độ và độ ẩm | 1-Wire (custom timing) |

> Có thể mở rộng thêm: DS18B20 (1-Wire), LCD I2C, v.v.

---

## Thay đổi cấu trúc `assignment/`

```
assignment/
├── lab6_ds1307/
│   ├── lab6_ds1307.json    ← Config + test cases
│   └── simulator.hex       ← Build từ bộ thư viện, enable DS1307 task
│
└── lab7_dht11/
    ├── lab7_dht11.json
    └── simulator.hex       ← Build từ bộ thư viện, enable DHT11 task
```

Bài tổng hợp sau này:
```
assignment/
└── lab10_combined/
    ├── lab10_combined.json
    └── simulator.hex       ← Enable DS1307 + DHT11 + LCD task cùng lúc
```

---

## Thay đổi cấu trúc `labX.json`

Thêm trường `simulator` để Pi biết cần setup gì cho Simulator trước khi test. Các trường còn lại (`decoders`, `test_cases`, `register_checks`) giữ nguyên, không thay đổi pipeline chấm điểm.

```json
{
  "target": "stm32f103c8",

  "simulator": {
    "hex": "simulator.hex",
    "setup": {
      "uart_baudrate": 115200,
      "uart_set": "TEMP:25,HUMI:60"
    }
  },

  "decoders": [...],
  "test_cases": [...],
  "register_checks": [...]
}
```

Trường `simulator.setup` chỉ dùng cho các sensor cần Pi **inject giá trị trước** (như DHT11 — Pi set temp/humidity qua UART vào Simulator). Sensor tương tác 2 chiều như DS1307 (sinh viên tự set time) thì `setup` để rỗng `{}`.

---

## Chiến lược chấm điểm bài sensor

Mỗi bài sensor phải chấm đủ **nhiều lớp**, không chỉ 1 điểm kiểm tra:

| Lớp | Ví dụ kiểm tra | Loại check |
|-----|---------------|------------|
| Giao thức | Địa chỉ I2C đúng, có write + read transaction | `i2c_addr_ack`, `regex` |
| Tín hiệu | Trigger pulse DHT11 đúng timing | `regex_value_range` |
| Data | 40 bit DHT11 hợp lệ, checksum đúng | `dht11_checksum` |
| UART output | Format đúng, giá trị nằm trong range hợp lệ | `regex`, `regex_value_range` |
| Register | Clock enable đúng peripheral | `register_checks` |

---

## Những gì không thay đổi

- Pipeline chấm điểm `main_grader.py` — không sửa flow
- Các loại check hiện có (`contains`, `regex`, `regex_value_range`, `i2c_addr_ack`, `dht11_checksum`)
- Cách sinh viên nộp bài qua GitHub Actions
- Cách kết quả hiển thị trên tab Actions
