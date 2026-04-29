# Chuẩn Viết Code (Coding Standard) cho Tầng Peripheral Driver

Tài liệu này quy định các nguyên tắc thiết kế, cách đặt tên và luồng dữ liệu (Data Flow) bắt buộc phải tuân thủ khi viết mới hoặc chỉnh sửa các module trong tầng `driver` của `simulator_fw`. Mục tiêu là đạt được tính **Tổng quát (Generic)**, **Hướng đối tượng (OOP in C)**, và **Tái sử dụng (Reusable)** cao nhất.

---

## 1. Chuẩn Đặt Tên (Naming Convention)

### 1.1. Tên File
Tất cả các file thuộc tầng ngoại vi phải bắt đầu bằng tiền tố `driver_`.
- Header: `driver_<module>.h` (VD: `driver_uart.h`, `driver_timer.h`)
- Source: `driver_<module>.c` (VD: `driver_uart.c`, `driver_timer.c`)

### 1.2. Kiểu Dữ Liệu (Struct & Enum)
Tất cả các `struct` và `enum` public phải dùng chữ thường, phân cách bằng dấu gạch dưới `_`, kết thúc bằng hậu tố `_t`. Nên dùng chung tiền tố của tên module (bỏ chữ driver_ đi cho ngắn gọn).
- Tên struct chính (Lớp OOP): `<module>_t` (VD: `uart_t`, `gpio_t`, `i2c_slave_t`).
- Enum Trạng thái: `<module>_state_t` (VD: `uart_state_t`).
- Enum Sự kiện: `<module>_event_t` (VD: `uart_event_t`).
- Enum Trạng thái trả về: `driver_status_t` (Sẽ định nghĩa chung).

### 1.3. Macro & Tiền tố Constant
Viết hoa toàn bộ, phân cách bằng dấu gạch dưới `_`.
- Các hằng số / Trạng thái: `<MODULE>_STATE_IDLE`, `<MODULE>_STATE_BUSY`.
- Lỗi (Error Codes): `DRIVER_OK`, `DRIVER_ERR_INVALID_ARG`.

### 1.4. Hàm (API Methods)
Bắt buộc bắt đầu bằng tên module (viết thường).
- API public khởi tạo: `<module>_init(...)`
- API public hoạt động: `<module>_start(...)`, `<module>_read(...)`, `<module>_write(...)`.
- Các hàm nội bộ (private) trong `.c`: Bắt buộc dùng tiền tố `prv_` và keyword `static`. (VD: `static void prv_uart_config_baudrate(...)`).

---

## 2. Thiết Kế Hướng Đối Tượng (OOP trong C)

Mỗi module phải có một struct cấu trúc đại diện cho bản thân ngoại vi đó (tương đương với một Object).

### 2.1. Tham số `instance` (Hardware Pointer)
Mọi struct `<module>_t` bắt buộc phải có một con trỏ trỏ thẳng tới thanh ghi ngoại vi của vi điều khiển, ví dụ:
```c
struct uart
{
    USART_TypeDef *instance; /* Bắt buộc: USART1, USART2, ... */
    volatile uart_state_t state;
    /* ... các thông số khác */
};
```
**Luật:** Tuyệt đối không hardcode ngoại vi (như `#define USE_USART1`) bên trong driver. Tất cả phải được truyền thông qua struct.

### 2.2. Con trỏ `this` trong tham số API
Tham số đầu tiên của mọi hàm public API bắt buộc phải là con trỏ của chính Object đó.
```c
void uart_start(uart_t *dev);
uint8_t uart_read_byte(uart_t *dev);
```

> **Ngoại lệ (Exception):** Các module thuộc dạng Singleton của hệ thống (như `RCC`, `NVIC`, `SysTick`) không cần tạo struct `instance` để tối ưu hóa bộ nhớ và tốc độ thực thi (Performance). Các hàm này được phép hoạt động như System Services tĩnh (VD: `rcc_enable(bus, peripheral)`).

---

## 3. Quản Lý Sự Kiện & Ngắt (Callbacks & Context)

Do viết bằng C, các Driver không thể tự biết ai đang gọi chúng. Nên phải dùng cơ chế Callback kèm Context (`void *ctx`).

### 3.1. Đăng ký Callback
Tất cả các hành vi xảy ra trong ngắt (như nhận xong 1 byte, truyền xong buffer) phải thông báo lên tầng trên bằng hàm Callback.
```c
typedef void (*uart_event_cb_t)(void *ctx, uart_event_t event, uint8_t data);

void uart_set_event_callback(uart_t *dev, uart_event_cb_t cb, void *ctx);
```
**Luật:** Biến `ctx` được lưu lại trong struct `uart_t`. Khi driver gọi hàm `cb`, nó phải truyền `ctx` ngược lại. Nhờ đó lớp Sensor biết chính xác object nào gây ra ngắt.

### 3.2. Handler của Driver
Logic ngắt thực tế (đọc thanh ghi SR, xóa cờ ngắt) phải được code gọn gàng thành một hàm `<module>_irq_handler(...)` nằm trong `driver_<module>.c`. 
File `stm32f10x_it.c` bên ngoài chỉ việc include header và gọi hàm này.

---

## 4. Phân Tách Phần Cứng (Hardware Decoupling)

**Luật Bất Thành Văn:** Tầng Driver của các thiết bị giao tiếp (`driver_timer`, `driver_uart`, `driver_i2c_slave`) **không được phép**:
1. Tự động bật RCC Clock (ví dụ `RCC_APB1PeriphClockCmd()`).
2. Tự động khởi tạo GPIO (ví dụ cấu hình PA9 thành TX, PA10 thành RX).

**Lý do:** Phần cứng board có thể thay đổi (dùng Remap pin PB6, PB7 thay vì PA9, PA10). Nếu nhét setup GPIO vào `driver_uart`, code sẽ chết cứng với 1 chân.
**Giải pháp:** Lớp trên (App hoặc Sensor) chịu trách nhiệm:
1. Gọi `rcc_enable()`.
2. Gọi `gpio_init()` để gán Mode phù hợp (Alternate Function...).
3. Truyền struct đã setup vào hàm `<module>_init()` để khởi tạo module.

---

## 5. Trạng Thái Trả Về Chung (Driver Status)

Nên định nghĩa một hệ thống mã lỗi chung (ví dụ trong `driver_common.h` hoặc tái sử dụng trực tiếp) để chuẩn hóa. Thay vì trả về `void`, các hàm init, start nên trả về:
```c
typedef enum {
    DRIVER_OK = 0,
    DRIVER_ERR_INVALID_ARG = -1,
    DRIVER_ERR_TIMEOUT = -2,
    DRIVER_ERR_BUSY = -3,
    DRIVER_ERR_NOT_SUPPORTED = -4
} driver_status_t;
```
