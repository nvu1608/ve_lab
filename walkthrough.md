# Tổng hợp các thay đổi Refactor Kiến trúc Simulator (Giai đoạn 2)

Tài liệu này tổng hợp lại toàn bộ quá trình tái cấu trúc mã nguồn giả lập (`simulator_fw`) nhằm đáp ứng chuẩn 3 lớp được định nghĩa trong `architecture_changes.md`.

## 1. Tái cấu trúc Thư mục

Từ source code cũ `ds1307_sim` (hơi rườm rà), toàn bộ hệ thống đã được chuyển hóa thành một bộ thư viện độc lập mang tên `simulator_fw` với cấu trúc kinh điển của hệ thống nhúng C/C++:

```text
ve_lab/simulator_fw/
├── app/
│   ├── app_config.h       ← Chứa #define để bật/tắt các sensor task
│   └── main.c             ← Entry point cực mỏng, chỉ gọi SystemInit và Start RTOS
├── driver/
│   ├── Inc/               ← Header file của phần cứng (Bare-metal)
│   └── Src/               ← Logic file của phần cứng (driver_i2c_slave, driver_gpio)
└── sensor/
    ├── Inc/               
    │   └── ds1307.h       ← Clean Header, chỉ expose hàm ds1307_sim_init()
    └── Src/
        └── ds1307.c       ← Gói trọn vẹn toàn bộ logic RTC, RTOS Task và I2C callback
```

Thư mục `freertos/` đã bị loại bỏ vì mã nguồn nhân RTOS đã nằm sẵn trong Base Project (`stm32_temp_sim`).

---

## 2. Các thay đổi tại Tầng App (App Layer)

- **Làm mỏng tuyệt đối**: Tầng app không còn chứa logic map phần cứng hay tạo task cụ thể.
- **`app_config.h`**: Trở thành file duy nhất giáo viên cần quan tâm để bật các chế độ lab:
  ```c
  #define ENABLE_DS1307 1
  #define ENABLE_DHT11  0
  ```
- **`main.c`**: Chỉ đơn giản kiểm tra các cờ `#define` này, gọi `init()` của sensor tương ứng rồi nhường quyền điều khiển lại cho hệ điều hành FreeRTOS (`vTaskStartScheduler()`).

---

## 3. Các thay đổi tại Tầng Sensor (Sensor Layer)

Đây là nơi thực hiện thay đổi lớn nhất (Refactor DS1307):
- **Gộp File**: Sáp nhập phần vỏ RTOS/Hardware (từ `app_ds1307.c` cũ) và phần lõi Logic (từ `ds1307_simu.c` cũ) vào chung một file duy nhất: `sensor/Src/ds1307.c`.
- **Encapsulation (Tính đóng gói)**: Thay vì phơi bày cấu trúc `ds1307_simu_t` hay các hàm xử lý BCD, triple-buffering ra ngoài, file `ds1307.h` giờ đây hoàn toàn "sạch sẽ". Các task khác hoặc `main.c` không thể chọc phá bộ đệm thời gian của DS1307, đảm bảo an toàn tuyệt đối khi chạy đa nhiệm.
- **Kế thừa các cơ chế an toàn**: Trong `ds1307.c`, các kỹ thuật cao cấp của người đồng nghiệp vẫn được giữ nguyên vẹn 100%:
  - Cơ chế **Triple-Buffering** để chống corrupt thời gian khi Master đọc qua I2C.
  - Cơ chế **Deferred Write** với `write_buf` để xử lý ghi trễ.
  - Cơ chế **Smart Task Loop** kết hợp Timeout 1s và Semaphore.

---

## 4. Các thay đổi tại Tầng Driver (Driver Layer)

- Giữ nguyên toàn bộ logic của người đồng nghiệp.
- Sắp xếp lại file vào 2 folder `Inc` và `Src` cho đúng quy chuẩn thư viện nhúng.

---

## 5. Cập nhật Makefile & Base Project (`stm32_temp_sim`)

Để biên dịch thành công hệ thống 3 lớp mới với RTOS và Standard Peripheral Library:
- **Xóa file cũ**: Đã xóa `stm32_temp_sim/Src/main.c` (để tránh xung đột với `simulator_fw/app/main.c`).
- **Sửa Linker/Compiler Paths (`Makefile`)**:
  - Gắn `../simulator_fw/app/main.c` và toàn bộ các file `.c` ở `driver/Src` và `sensor/Src` vào biến biên dịch `C_SOURCES`.
  - Khai báo thêm `-I` (Include directories) để trình biên dịch tìm thấy các file `.h` mới của hệ thống `simulator_fw`.
- **Sửa GCC_PATH**: Fix cứng đường dẫn trình biên dịch (Toolchain) thành chuẩn ARM64 chạy trên Raspberry Pi:
  ```make
  GCC_PATH = /home/pi/ve_lab/arm_gcc/arm-gnu-toolchain-15.2.rel1-aarch64-arm-none-eabi/bin
  ```

---

## Kết quả
Hệ thống hiện tại (bản đồ thư mục và cấu hình Build) đã khớp 100% với lộ trình kiến trúc mới, có khả năng mở rộng thoải mái cho nhiều bài lab tổng hợp sau này.
