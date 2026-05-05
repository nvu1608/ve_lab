# Chuẩn Kiến Trúc và Viết Code (Architecture & Coding Standard) - `simulator_fw`

Tài liệu này quy định các nguyên tắc thiết kế, cấu trúc thư mục và cách viết code cho toàn bộ project `simulator_fw`. Mục tiêu là đạt được tính **Module hóa (Modularity)**, **Đóng gói (Encapsulation)**, và **Dễ mở rộng (Scalability)**.

---

## 1. Kiến Trúc Phân Lớp (Layered Architecture)

Hệ thống được chia thành 4 lớp rõ ràng, mỗi lớp chỉ biết đến lớp ngay bên dưới nó:

1.  **App Layer (`app_layer/`)**:
    *   Trách nhiệm: Điều phối cao nhất, khởi tạo hệ thống và chạy Scheduler.
    *   File chính: `main.c`, `app_cfg.h`.
    *   Quy tắc: Không chứa logic điều khiển thanh ghi hay logic mô phỏng sensor. Chỉ gọi hàm `init` từ Project Layer.

2.  **Project Layer (`project_layer/`)**:
    *   Trách nhiệm: Là lớp "Glue" (Gắn kết). Kết nối Driver cụ thể với Sensor cụ thể trên một sơ đồ chân (Pinout) cụ thể.
    *   Cấu trúc: Mỗi project là một thư mục (VD: `ds1307_project/`).
    *   File: `_project.c/h` (Logic gắn kết), `_project_cfg.h` (Thông số phần cứng).

3.  **Sensor Layer (`sensor_layer/`)**:
    *   Trách nhiệm: Mô phỏng logic của linh kiện (Register map, behavior).
    *   Cấu trúc: Mỗi sensor là một thư mục (VD: `ds1307/`).
    *   File: `_api.h` (Interface cho App), `_sim.c/h` (Lõi mô phỏng và RTOS Task).

4.  **Driver Layer (`driver_layer/`)**:
    *   Trách nhiệm: Điều khiển trực tiếp ngoại vi MCU (I2C, GPIO, UART...).
    *   Đặc điểm: Thuần túy hardware, không biết về sensor hay logic nghiệp vụ.

---

## 2. Chuẩn Đặt Tên (Naming Convention)

### 2.1. Tên File
*   **Driver**: `driver_<name>.c/h` (VD: `driver_i2c_slave.c`).
*   **Sensor**:
    *   `<name>_api.h`: Định nghĩa interface public.
    *   `<name>_sim.c/h`: Định nghĩa logic mô phỏng.
*   **Project**:
    *   `<name>_project.c/h`: Logic khởi tạo dự án.
    *   `<name>_project_cfg.h`: Cấu hình tài nguyên phần cứng.

### 2.2. Kiểu Dữ Liệu & Hàm
*   **Struct & Enum**: Kết thúc bằng hậu tố `_t`. (VD: `ds1307_t`, `i2c_slave_t`).
*   **Hàm Public API**: Bắt đầu bằng tên module. (VD: `ds1307_set_time()`, `gpio_write()`).
*   **Hàm Private**: Bắt buộc dùng tiền tố `prv_` và keyword `static`. (VD: `static void prv_ds1307_tick_1s()`).

---

## 3. Quản Lý Cấu Hình (Configuration Management)

Hệ thống sử dụng cơ chế phân tách cấu hình để đảm bảo tính Module:

1.  **Cấu hình Hệ thống (`app_cfg.h`)**:
    *   Chỉ chứa các macro bật/tắt (Enable/Disable) các project/sensor.
    *   Ví dụ: `#define ENABLE_DS1307 1`.

2.  **Cấu hình Project (`_project_cfg.h`)**:
    *   Chứa toàn bộ thông số phần cứng (Pin, Port, Instance, IRQ Priority).
    *   Mục tiêu: Khi đổi chân cắm, chỉ cần sửa file này, không động vào logic code.

---

## 4. Nguyên Tắc Thiết Kế (Design Principles)

### 4.1. Hướng Đối Tượng (OOP in C)
*   Mỗi module phải đại diện bằng một struct (Object).
*   Mọi hàm API public phải nhận con trỏ của Object đó làm tham số đầu tiên (`this` pointer).
    ```c
    void ds1307_set_time(ds1307_t *dev, const ds1307_time_t *time);
    ```

### 4.2. Đóng Gói RTOS (RTOS Encapsulation)
*   Sensor Layer chịu trách nhiệm tự "đóng gói" logic mô phỏng vào một FreeRTOS Task.
*   Tầng App chỉ việc gọi hàm khởi tạo, Sensor sẽ tự chạy ngầm.

### 4.3. Phân Tách Phần Cứng (Hardware Decoupling)
*   **Driver không được tự ý bật Clock hay cấu hình GPIO**. 
*   Việc bật RCC Clock và gán chân GPIO phải được thực hiện ở **Project Layer**. Driver chỉ nhận vào instance đã được setup sẵn.

### 4.4. Cơ Chế Callback
*   Driver giao tiếp ngược lên Sensor thông qua các hàm Callback kèm Context (`void *ctx`).
*   Tuyệt đối không gọi trực tiếp hàm của Sensor từ trong Driver.

---

## 5. Trạng Thái Trả Về (Return Status)

Tất cả các hàm khởi tạo và thực thi quan trọng nên trả về `driver_status_t` để báo lỗi:
```c
typedef enum {
    DRIVER_OK = 0,
    DRIVER_ERR_INVALID_ARG = -1,
    DRIVER_ERR_TIMEOUT = -2,
    DRIVER_ERR_BUSY = -3
} driver_status_t;
```
