# Tài liệu Mô phỏng Cảm biến DHT11 (DHT11 Simulator)

## 1. Giới thiệu
Module `dht11_sim` mô phỏng một cảm biến DHT11 hoạt động trên vi điều khiển STM32F103. Quá trình giao tiếp với Host (board sinh viên) sử dụng giao thức 1-wire chuẩn của DHT11 thông qua một chân tín hiệu duy nhất.

## 2. Sơ đồ Kiến trúc Hệ thống

Mô hình hoạt động dựa trên kiến trúc 3 lớp (Driver - Sensor - App) đảm bảo tính tách biệt và hiệu suất cao nhờ RTOS.

```text
 ┌──────────────┐  dht11_sim_init()   ┌──────────────────────┐
 │  APP LAYER   │ ───────────────────►  │   SENSOR LAYER       │
 │ (main.c)     │  dht11_sim_run()      │   FreeRTOS Task      │
 │              │ ───────────────────►  │  ┌────────────────┐  │
 │ app_config.h │  dht11_sim_set_data() │  │ block semaphore│  │
 │              │ ───────────────────►  │  │ → build data   │  │
 └──────────────┘                       │  │ → phát 40 bit  │  │
                                        │  └───────┬────────┘  │
                                        └──────────┼───────────┘
                                                   │ register direct
                                         ┌─────────▼──────────┐
                                         │  DRIVER LAYER      │
                                         │ TIM1(OC) TIM2(IC)  │
                                         │ PA0 (Single-wire)  │
                                         └────────────────────┘
```

## 3. Đặc tả Phần cứng & Giao thức

### 3.1. Cấu hình chân tín hiệu (Pinout)
- **Chân giao tiếp:** `PA0`
- **Cơ chế Switch Mode:** Chân `PA0` được thay đổi chế độ hoạt động linh hoạt tùy theo trạng thái giao thức:
    - **IC Mode (Input Capture):** Cấu hình `GPIO_Mode_IPU`. Sử dụng để phát hiện tín hiệu Start (mức LOW kéo dài) từ Host.
    - **TX Mode (Output Compare):** Cấu hình `GPIO_Mode_Out_OD`. Sử dụng để phản hồi (Response) và gửi dữ liệu 40-bit.

### 3.2. Timing Engine
Hệ thống sử dụng hai Timer phối hợp để đạt độ chính xác micro-giây (µs):
- **TIM2 Channel 1 (Input Capture):** Đo độ rộng xung Start signal từ Host (yêu cầu từ 18ms đến 30ms).
- **TIM1 Channel 1 (Output Compare):** Hoạt động như một "timing engine" để điều khiển việc lật trạng thái chân `PA0` chính xác theo từng bit dữ liệu.

### 3.3. Thông số Timing chuẩn (µs)
| Giai đoạn | Thời gian (µs) | Mô tả |
|---|---|---|
| **Response LOW** | 80 | Simulator kéo LOW báo hiệu sẵn sàng |
| **Response HIGH** | 80 | Simulator thả HIGH chuẩn bị gửi data |
| **Bit 0 HIGH** | 26 - 28 | Độ rộng mức HIGH để biểu diễn bit 0 |
| **Bit 1 HIGH** | 70 | Độ rộng mức HIGH để biểu diễn bit 1 |
| **Bit LOW** | 50 | Khoảng cách mức LOW giữa các bit |

## 4. Cơ chế Hoạt động (Logic Flow)

1.  **Chờ đợi (Wait):** Task DHT11 bị block tại `Binary Semaphore`. TIM2 (IC) được kích hoạt để canh Start signal.
2.  **Kích hoạt (Trigger):** Khi Host kéo LOW `PA0` đủ lâu (18ms), IC ISR sẽ `Give` Semaphore để đánh thức Task.
3.  **Xây dựng Frame:** Task lấy dữ liệu (Nhiệt độ/Độ ẩm) từ `Queue` (nếu có cập nhật mới) và tính toán Checksum.
4.  **Phát dữ liệu (Transmit):**
    - Chuyển `PA0` sang Output Compare.
    - TIM1 OC điều khiển logic lật chân `PA0` để tạo ra chuỗi 40-bit.
5.  **Hoàn tất:** Sau khi gửi xong bit thứ 40, hệ thống chuyển `PA0` về lại IC Mode và chờ đợi lượt truy vấn tiếp theo.

## 5. Nhật ký Chuyển đổi (Migration Log)

### 5.1. Cấu trúc thư mục mới
Toàn bộ mã nguồn cũ được chuyển vào kiến trúc 3 lớp:
*   `driver_timer.c/h`: Driver ngoại vi dùng chung cho toàn hệ thống.
*   `dht11.c/h`: Logic giả lập cảm biến đặt tại `simulator_fw/sensor/`.

### 5.2. Khởi tạo & Cấu hình
- **Loại bỏ `dht11_app.c`**: Việc khởi tạo Task và cấu hình phần cứng hiện được gộp vào hàm duy nhất `dht11_sim_init()`.
- **Kích hoạt module**: Sử dụng flag `#define ENABLE_DHT11 1` trong `app_config.h`.

---

## 6. Các API chính

```c
/* Khởi tạo simulator, tạo task và chuẩn bị phần cứng */
void dht11_sim_init(void);

/* Cập nhật dữ liệu mô phỏng (gọi từ App hoặc Script chấm điểm) */
int dht11_sim_set_data(const DHT11_Data_t *data);
```
