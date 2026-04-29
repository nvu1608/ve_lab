# Embedded Hardware Autograder — Project Context cho AI Assistant

> **LƯU Ý DÀNH CHO AI ASSISTANT**: Đây là file quan trọng nhất bạn cần đọc khi bắt đầu một session mới. Nó chứa ngữ cảnh (context) tổng thể về kiến trúc hệ thống và quy định code của dự án.

> **TÀI LIỆU CHI TIẾT**: Toàn bộ các tài liệu thiết kế chi tiết (như Overview, Architecture, Review) đã được quy hoạch gọn gàng vào thư mục `docs/`. Bạn hãy đọc file [docs/index.md](file:///home/uynv/VELab_Hardware/ve_lab/docs/index.md) để tra cứu các tài liệu liên quan trong dự án.

## 1. Tổng quan Dự án (Overview)
Đây là hệ thống **Tự động chấm điểm (Autograder)** bài tập Thực hành Hệ thống nhúng (Embedded Systems) chạy trên **Raspberry Pi 3**, tích hợp với **GitHub Actions Self-hosted Runner**.
Hệ thống sử dụng **2 vi điều khiển STM32F103C8T6**:
- **Student MCU**: Chạy mã nguồn C do sinh viên nộp lên GitHub.
- **Simulator MCU**: Chạy firmware đóng vai trò là "Thiết bị ngoại vi giả lập" (Ví dụ: đóng vai trò là cảm biến DS1307, DHT11) để tương tác với Student MCU.

## 2. Kiến trúc Code & Thư mục (Directory Structure)

* `logic/`: Chứa mã nguồn Python chấm điểm. Orchestrator chính là `main_grader.py`. Nó điều khiển việc nạp code, reset chip, gọi logic analyzer (sigrok) và check thanh ghi (pyOCD).
* `stm32_temp_stu/`: Project template (Makefile, FreeRTOS, StdPeriph) dùng để compile code của sinh viên.
* `stm32_temp_sim/`: Project template dùng để compile firmware cho Simulator MCU.
* `simulator_fw/`: **(QUAN TRỌNG)** Bộ mã nguồn cốt lõi (Source code) cho Simulator MCU, được viết theo chuẩn **Kiến trúc 3 lớp (3-layer Architecture)**:
  - **App Layer (`app/`)**: Cực kỳ mỏng. Chỉ chứa `app_config.h` (dùng `#define` để bật/tắt module) và `main.c` (khởi tạo và chạy RTOS Scheduler). KHÔNG chứa logic phức tạp ở đây.
  - **Sensor Layer (`sensor/`)**: Mỗi cảm biến (VD: `ds1307.c`) là một gói hoàn chỉnh chứa logic tính toán (BCD, Tick), FreeRTOS Task và khởi tạo phần cứng. Code luôn xử lý triệt để race-condition (dùng Mutex/Semaphore và Triple-Buffering).
  - **Driver Layer (`driver/`)**: Mã nguồn giao tiếp phần cứng cơ bản (I2C, GPIO, RCC...). Không được chứa logic cụ thể của từng cảm biến.
* `assignment/`: Chứa cấu hình test cases (`labX.json`) và file `simulator.hex` (được pre-build từ `stm32_temp_sim` + `simulator_fw`) cho mỗi bài Lab.

## 3. Quy chuẩn & Lưu ý khi viết code (Coding Guidelines)

1. **Phát triển Simulator Firmware**:
   - Khi thêm cảm biến mới (VD: DHT11), tuyệt đối tuân thủ kiến trúc 3 lớp: Tạo file `sensor/Src/dht11.c` chứa toàn bộ RTOS Task và cấu hình ngoại vi. Chỉ thêm đúng `#define ENABLE_DHT11` vào `app_config.h`.
   - Tránh việc chia nhỏ logic cảm biến ra nhiều file rải rác.
   - Luôn sử dụng FreeRTOS (Task, Semaphore, Mutex) để xử lý thay vì vòng lặp `while()` hoặc hàm delay block CPU.
2. **Quy chuẩn viết Peripheral Driver (Tầng Driver)**:
   - Các module driver phải được viết theo chuẩn **OOP trong C** (truyền struct `<module>_t *dev`).
   - Hỗ trợ truyền Event Callback kèm Context (`void *ctx`).
   - **Tuyệt đối KHÔNG** bật RCC hoặc khởi tạo GPIO cứng bên trong tầng Driver.
   - Tránh hardcode tham số. Mọi nguyên tắc thiết kế xem tại: [docs/driver_coding_standard.md](file:///home/uynv/VELab_Hardware/ve_lab/docs/driver_coding_standard.md).
3. **Cập nhật Python Grader (`logic/`)**:
   - Chú ý luồng chạy đồng bộ (Synchronous logic): Reset chân NRST, check thanh ghi (khi chip dừng), thả NRST, bắt sóng Logic Analyzer (NON-BLOCKING).
   - Không phá vỡ luồng `main_grader.py`. Mọi xử lý tín hiệu mới nên được thêm vào `check_logic.py` (chấm điểm từ file CSV).

## 4. Các lệnh Terminal thường dùng
* Chạy test chấm điểm 1 bài lab thủ công:
  `python3 /home/pi/ve_lab/logic/main_grader.py --lab lab6_ds1307`
* Chạy test thanh ghi (SWD) độc lập để debug:
  `python3 /home/pi/ve_lab/logic/check_reg.py --lab lab6_ds1307`
* Build Simulator Firmware (nếu sửa code trong `simulator_fw/`):
  `cd stm32_temp_sim && make clean && make -j4`
