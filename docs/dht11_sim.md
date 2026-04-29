# Tài liệu Mô phỏng Cảm biến DHT11 (DHT11 Simulator)

## 1. Giới thiệu
Module `dht11_sim` được thiết kế để mô phỏng một cảm biến DHT11 hoạt động độc lập trên vi điều khiển STM32F103. Quá trình giao tiếp với Host (board sinh viên) sử dụng 1 dây tín hiệu duy nhất theo giao thức chuẩn của DHT11.

## 2. Phần cứng sử dụng
- **Chân giao tiếp:** `PA0` (sử dụng Open-Drain, đòi hỏi điện trở kéo lên - pull-up ngoài hoặc internal).
- **Timer IC (Input Capture):** `TIM2 CH1` được dùng để đo xung đầu vào từ Host (nhận biết Start Signal: kéo Low 18ms).
- **Timer OC (Output Compare):** `TIM1 CH1` được dùng như "timing engine" chính xác tới từng micro-giây để xuất 40-bit data (bao gồm nhiệt độ, độ ẩm, checksum) sau khi nhận được tín hiệu bắt đầu hợp lệ.

## 3. Kiến trúc Phần mềm
Kiến trúc mô phỏng cảm biến bao gồm:
1. **Hardware / ISR Level:** Các ngắt Timer (`TIM1_UP`, `TIM1_CC`, `TIM2_IRQHandler`) phục vụ việc thu thập và phát tín hiệu pulse.
2. **RTOS Level:** Một FreeRTOS Task được tạo ra làm State Machine vòng lặp vô hạn. Khi Host kích hoạt ngắt TIM2, ISR đẩy Binary Semaphore sang cho Task để báo hiệu việc truyền dữ liệu.
3. **Queue Level:** Thông số cấu hình độ ẩm / nhiệt độ được cập nhật thread-safe thông qua hàm Set Data và ghi đè (overwrite) vào một Queue có độ dài 1.

## 4. Nhật ký Chuyển đổi (Migration Log) so với bản cũ
Trong quá trình chuyển đổi kiến trúc từ thư mục `dht11_sim/` độc lập vào kiến trúc 3 lớp của `simulator_fw`, các thay đổi lớn sau đã được thực hiện:

### 4.1. Cấu trúc thư mục mới
Toàn bộ mã nguồn cũ được gom gọn về 2 lớp cốt lõi để duy trì tính nhất quán (không tạo file riêng rẽ ở lớp App):
* `timer_driver.c/h` -> Đổi tên và chuyển về `simulator_fw/driver/Src/driver_timer.c` và `simulator_fw/driver/Inc/driver_timer.h`.
* `dht11_sim.c/h` -> Cập nhật Header và đổi tên ngắn gọn thành `simulator_fw/sensor/Src/dht11.c` và `simulator_fw/sensor/Inc/dht11.h`.

### 4.2. Khử lớp `dht11_app` không cần thiết
* **Trước đây:** Có thư mục `App` chứa file `dht11_app.c/h`, trong đó định nghĩa hàm khởi tạo ưu tiên tác vụ (task priority) và gọi cấu hình sensor.
* **Hiện tại:** Loại bỏ hoàn toàn `dht11_app.c`. Đưa toàn bộ cấu hình vào thẳng một hàm khởi tạo API duy nhất `dht11_sim_init()`. Hàm này được gọi từ lớp app (file `main.c`). Cách tiếp cận này giống hệt cách cấu hình `ds1307`.

### 4.3. Quản lý trạng thái bằng Macro
* Module được kích hoạt từ `simulator_fw/app/app_config.h` với flag `#define ENABLE_DHT11 1` và chạy `dht11_sim_init()` ở `main.c` giống như các sensor khác.

### 4.4. Đổi tên Include
Trong mã nguồn `dht11.c`, `#include "timer_driver.h"` và `#include "dht11_sim.h"` được cập nhật tương ứng thành `#include "driver_timer.h"` và `#include "dht11.h"`. Tương tự, tên API cũng đổi từ `dht11_sim_config()` thành nội bộ và mở rộng thông qua API chuẩn `dht11_sim_init()`.
