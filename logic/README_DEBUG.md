# Hướng dẫn Debug & Sử dụng độc lập các module Logic

Tài liệu này hướng dẫn cách kiểm tra (test) từng thành phần trong hệ thống Autograder một cách độc lập mà không cần thông qua luồng tự động của file `main.py`. Việc này giúp cô lập lỗi và debug nhanh hơn.

---

## 1. Biên dịch Firmware (`builder.py`)
Sử dụng để kiểm tra quá trình biên dịch (compile) mã nguồn sinh viên và simulator.

*   **Lệnh thực hiện (qua Python REPL):**
    ```bash
    # Build cho sinh viên
    python3 -c "import sys; sys.path.append('.'); import builder; builder.build_student('lab6_ds1307', '/home/pi/ve_lab')"

    # Build cho Simulator (có kèm flag tự động nhận diện cảm biến)
    python3 -c "import sys; sys.path.append('.'); import builder; builder.build_simulator('lab6_ds1307')"
    ```

---

## 2. Kiểm tra thanh ghi MCU (`check_reg.py`)
Sử dụng pyOCD để đọc trực tiếp các thanh ghi cấu hình của STM32 qua giao thức SWD.

*   **Lệnh thực hiện:**
    ```bash
    # Chạy kiểm tra theo cấu hình trong JSON của bài Lab
    python3 /home/pi/ve_lab/logic/check_reg.py --lab lab6_ds1307
    ```
*   **Tham số mở rộng:**
    *   `--serial`: Chỉ định ID của ST-Link (nếu máy có nhiều ST-Link).
    *   `--target`: Chỉ định dòng chip (mặc định `stm32f103c8`).

---

## 3. Chấm điểm Logic từ CSV (`check_logic.py`)
Sử dụng để kiểm tra bộ máy chấm điểm dựa trên file dữ liệu `result.csv` đã có sẵn. Rất hữu ích khi cần tinh chỉnh Test Case trong JSON.

*   **Lệnh thực hiện:**
    ```bash
    # Chấm điểm dựa trên file logic/result.csv mặc định
    python3 /home/pi/ve_lab/logic/check_logic.py --lab lab6_ds1307
    ```
*   **Tham số mở rộng:**
    *   `--csv`: Chỉ định đường dẫn tới file CSV khác.
    *   `--json`: Chỉ định file cấu hình JSON cụ thể.

---

## 4. Điều khiển Capture tín hiệu (`sigrok.py`)
Sử dụng để test khả năng kết nối và bắt tín hiệu của Logic Analyzer.

*   **Lệnh thực hiện (test nhanh qua sigrok-cli):**
    ```bash
    sigrok-cli --driver saleae-logic --config samplerate=1m --samples 1m --channels D0,D1,D2
    ```

---

## 5. Sử dụng `main.py` ở chế độ Manual
Tận dụng các Flag đã được module hóa để chạy từng Phase riêng biệt.

*   **Chỉ Build và Flash (Không chấm điểm):**
    ```bash
    python3 /home/pi/ve_lab/logic/main.py --lab lab6_ds1307 --build --flash
    ```
*   **Chỉ thực hiện Register Check:**
    ```bash
    python3 /home/pi/ve_lab/logic/main.py --lab lab6_ds1307 --reg
    ```
*   **Chỉ thực hiện Capture và Grade (Khi đã nạp code xong):**
    ```bash
    python3 /home/pi/ve_lab/logic/main.py --lab lab6_ds1307 --capture --grade
    ```

---

## Lưu ý chung
*   Mọi lệnh nên được thực hiện tại thư mục gốc của dự án hoặc thư mục `logic/`.
*   Đảm bảo các chân NRST (Reset) không bị giữ ở mức thấp (0V) khi đang thực hiện Register Check độc lập.
