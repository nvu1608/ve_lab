# Phân tích sự khác biệt giữa `stm32_temp_sim` và `stm32_temp_stu`

Dưới đây là phân tích chi tiết về những thay đổi khi chuyển từ template gốc `stm32_temp_stu` sang template `stm32_temp_sim` (đã được tích hợp hệ điều hành thời gian thực FreeRTOS):

## 1. Thêm thư mục và file mới
*   **Thư mục `FreeRTOS/`**: Toàn bộ source code của FreeRTOS đã được thêm vào thư mục này, bao gồm phần lõi (`tasks.c`, `queue.c`, `list.c`, v.v.) và phần port cho kiến trúc ARM Cortex-M3 (`port.c`, `heap_4.c`).
*   **File `Src/syscalls.c`**: Được thêm vào để hỗ trợ các hàm thư viện chuẩn C (như `printf`, `malloc`) hoạt động ở mức low-level (bare-metal) trên vi điều khiển STM32.

## 2. Thay đổi trong cấu hình build (`Makefile`)
*   **Thêm Source files**: Đã bổ sung các file `.c` của thư viện FreeRTOS và file `syscalls.c` vào danh sách `C_SOURCES` để tiến hành biên dịch.
*   **Thêm Include paths**: Bổ sung các đường dẫn thư viện (`include`) của FreeRTOS (`-IFreeRTOS/include`, `-IFreeRTOS/portable/GCC/ARM_CM3`) vào biến `C_INCLUDES`.
*   **Cập nhật cấu hình GCC Toolchain**: Cập nhật cách xác định đường dẫn compiler (`GCC_PATH`), hỗ trợ lấy từ biến môi trường `ARM_GCC_PATH`.
*   **Hỗ trợ in số thực (`printf float`)**: Thêm biến `PRINTF_FLOAT` và tự động nối cờ linker `LDFLAGS += -u _printf_float` nếu tính năng này được bật (`PRINTF_FLOAT=1`).
*   **Cập nhật Linker Flags**: Chuyển từ việc link thư viện nosys trong phần `LIBS` (`-lnosys`) sang dùng cờ `-specs=nosys.specs` trong biến `LDFLAGS`.
*   **Thay đổi địa chỉ Flash**: Trong lệnh `make flash`, địa chỉ nạp file bin vào flash được đổi từ `0x08003000` (thường dùng chung với custom bootloader) về địa chỉ mặc định của STM32 là `0x08000000`.

## 3. Thay đổi trong source code hệ thống và ngắt
*   **`Src/system_stm32f10x.c`**: Thay đổi địa chỉ khởi tạo bảng vector ngắt (`VTOR`) từ `0x08003000` về địa chỉ chuẩn `FLASH_BASE` (`0x08000000`). Điều này đồng bộ với việc thay đổi địa chỉ nạp trong lệnh `make flash`.
*   **`Src/stm32f10x_it.c`**:
    *   Thêm dòng `#include "FreeRTOS.h"`.
    *   Bọc các hàm xử lý ngắt hệ thống mặc định (`SVC_Handler`, `PendSV_Handler`, `SysTick_Handler`) bằng chỉ thị `#ifndef`. Điều này rất quan trọng vì FreeRTOS sẽ tự cung cấp (implement) các hàm ngắt này để phục vụ việc chuyển đổi ngữ cảnh (context switching) và tạo bộ đếm thời gian (tick). Các hàm này thường được map sang `vPortSVCHandler`, `xPortPendSVHandler`, `xPortSysTickHandler` trong file cấu hình của FreeRTOS.

## 4. Cấu trúc lại toàn bộ chương trình chính (`Src/main.c`)
Toàn bộ logic của file `main.c` cũ (chứa code giao tiếp I2C với module RTC DS1307) đã bị xóa và thay thế bằng các ví dụ mẫu (template) để chạy FreeRTOS:
*   **Thư viện FreeRTOS**: Bao gồm các thư viện cần thiết như `FreeRTOS.h`, `task.h`, `semphr.h`, `timers.h`, `queue.h`.
*   **Khởi tạo cơ bản**: Cấu hình lại hàm `usart1_init` và `usart1_send_string` (thay cho các hàm `Uart_Config`/`SendString` cũ) để phục vụ việc in log debug ra UART.
*   **Tích hợp hàm `printf`**: Do đã có `syscalls.c`, template mới sử dụng thư viện `stdio.h` giúp cho việc in log dễ dàng qua hàm `printf()`.
*   **Các kịch bản mẫu (Macros)**: Source code được chuẩn bị sẵn thành nhiều kịch bản test FreeRTOS khác nhau, người dùng có thể dễ dàng bật/tắt bằng cách bỏ comment các định nghĩa macro (`#define`):
    *   `GLOBAL` & `CRITICAL_SECTION`: Ví dụ về việc tạo nhiều task cùng truy cập vào một biến toàn cục `g_sum` và minh họa cơ chế bảo vệ vùng găng.
    *   `SEMAPHORE_V1` & `SEMAPHORE_V2`: Minh họa cách sử dụng Semaphore nhị phân (Binary Semaphore) để đồng bộ hóa luồng giữa các task.
    *   `QUEUE_V1` & `QUEUE_V2`: Ví dụ giao tiếp giữa các task sử dụng Hàng đợi (Queue), bao gồm truyền các biến nguyên thuỷ (như `uint32_t`) cho tới truyền các cấu trúc dữ liệu (`struct`).
*   **Các hàm Hook của FreeRTOS**: Cung cấp sẵn các hàm hook (callback) để xử lý các sự kiện hệ thống hoặc lỗi khi sử dụng RTOS:
    *   `vApplicationIdleHook`: Chạy khi CPU rảnh (không có task nào cần xử lý), gọi lệnh Wait For Interrupt `__WFI()` để tiết kiệm năng lượng.
    *   `vApplicationMallocFailedHook`: Hàm bắt lỗi để chẩn đoán khi không đủ bộ nhớ cấp phát (heap).
    *   `vApplicationStackOverflowHook`: Hàm chẩn đoán bắt lỗi tràn stack của một task cụ thể.