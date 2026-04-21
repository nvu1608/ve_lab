# Embedded Hardware Autograder — Tổng quan hệ thống

## Cấu trúc thư mục trên Pi

```
/home/pi/ve_lab/
├── arm_gcc/
│   └── arm-gnu-toolchain-15.2.rel1-aarch64-arm-none-eabi/
│       └── bin/                    ← arm-none-eabi-gcc, objcopy, ...
│
├── assignment/                     ← Repo giáo viên (private) — config + firmware mỗi lab
│   ├── lab1_gpio/
│   │   ├── lab1.json               ← Config + test cases
│   │   └── simulator.hex           ← Firmware cho STM32 Simulator
│   ├── lab2_uart/
│   │   └── lab2.json
│   ├── lab3_timer/
│   │   └── lab3.json
│   ├── lab4_spi/
│   │   └── lab4.json
│   └── lab5_i2c/
│       ├── lab5.json
│       └── simulator.hex
│
├── docs/
│   ├── action_runner/
│   │   └── setup_runner.txt
│   ├── arm_gcc_stlink/
│   │   └── setup.txt
│   └── gdb_pyocd/
│       ├── gdb.txt
│       └── pyocd.txt
│
├── logic/                          ← Toàn bộ Python grader
│   ├── main_grader.py              ← Orchestrator chính
│   ├── sigrok.py                   ← Logic analyzer capture + decode
│   ├── check_logic.py              ← Chấm điểm từ signal data (CSV)
│   ├── check_reg.py                ← Kiểm tra thanh ghi MCU qua SWD
│   └── result.csv                  ← Output capture của sigrok
│
├── stm32_temp_stu/                 ← Project template build cho Student MCU
│   ├── Src/
│   │   ├── main.c                  ← Được copy từ repo sinh viên vào đây
│   │   ├── stm32f10x_it.c
│   │   └── system_stm32f10x.c
│   ├── Inc/
│   │   ├── stm32f10x_conf.h
│   │   └── stm32f10x_it.h
│   ├── Drivers/
│   │   ├── CMSIS/
│   │   └── STM32F10x_StdPeriph_Driver/
│   ├── Makefile                    ← GCC_PATH trỏ tới arm_gcc/ bên trên
│   ├── startup_stm32f103xb.s
│   ├── STM32F103C8Tx_FLASH.ld
│   └── build/
│       ├── template_make.hex       ← Flash vào STM32 Student
│       ├── template_make.elf
│       ├── template_make.bin
│       └── template_make.map
│
└── autograder.log                  ← Log toàn bộ quá trình chấm
```

---

## Cấu trúc repo sinh viên — `my-autograder` (public)

```
my-autograder/
├── .github/
│   └── workflows/
│       └── autograder.yml          ← CI/CD workflow
├── lab1_gpio/main.c
├── lab2_uart/main.c
├── lab3_timer/main.c
├── lab4_spi/main.c
└── lab5_i2c/main.c
```

Kết quả chấm điểm hiển thị trực tiếp trên tab **Actions** của GitHub — không cần push `result.txt` lên repo.

---

## Phần cứng

| Component | Vai trò | Ghi chú |
|---|---|---|
| Raspberry Pi 3 | Não trung tâm | Build, flash, orchestrate, chấm điểm |
| STM32F103C8T6 — Student | Chạy code sinh viên | Flash qua STLink serial `37FF71064E573436DB011543` |
| STM32F103C8T6 — Simulator | Giả lập sensor ngoại vi | Flash qua STLink serial `10002D00182D343632525544` |
| STLink v2 × 2 | Flash + debug qua SWD | Phân biệt bằng serial number |
| Logic Analyzer (fx2lafw) | Probe bus, decode protocol | sigrok-cli, samplerate 2MHz |
| CH446Q Switch Matrix | Routing GPIO linh hoạt | Config qua routing JSON mỗi lab (Phase 2) |

**Kết nối NRST:**
```
Pi GPIO 529 → NRST STM32 Student
Pi GPIO 539 → NRST STM32 Simulator
```
Dùng sysfs (`/sys/class/gpio/`) để hold/release reset đồng bộ cả 2 MCU.

**Kênh Logic Analyzer:**
```
D0 = I2C SCL    D4 = SPI MOSI
D1 = I2C SDA    D5 = SPI CLK
D2 = UART TX    D6 = SPI CS
D3 = DHT11      D7 = PWM
```

**ARM GCC Toolchain:**
```
/home/pi/ve_lab/arm_gcc/arm-gnu-toolchain-15.2.rel1-aarch64-arm-none-eabi/bin/
```
Đã hardcode trong `stm32_temp_stu/Makefile` tại biến `GCC_PATH`.

---

## CI/CD — GitHub Actions Self-hosted Runner

Sinh viên push code → GitHub Actions trigger → Runner trên Pi nhận job → chạy `main_grader.py`.

**Runner:** cài tại `~/actions-runner`, chạy như systemd service.

### Cài đặt runner lần đầu

**Bước 1 — Lấy token trên GitHub:**

Vào repo `my-autograder` → **Settings** → **Actions** → **Runners** → **New self-hosted runner**
- OS: **Linux**
- Architecture: **ARM64**

Giữ trang đó mở để lấy token.

**Bước 2 — Cài runner trên Pi:**

```bash
mkdir -p ~/actions-runner && cd ~/actions-runner

# Tải runner ARM64 (kiểm tra phiên bản mới nhất trên trang GitHub ở bước 1)
curl -o actions-runner-linux-arm64.tar.gz -L \
  https://github.com/actions/runner/releases/download/v2.323.0/actions-runner-linux-arm64-2.323.0.tar.gz

tar xzf actions-runner-linux-arm64.tar.gz
```

**Bước 3 — Cấu hình (dùng token từ GitHub):**

```bash
./config.sh --url https://github.com/<username>/my-autograder --token <TOKEN>
```

Nhấn Enter cho tất cả câu hỏi (tên, group, labels, work folder).

**Bước 4 — Cài như systemd service:**

```bash
sudo ./svc.sh install
sudo ./svc.sh start

# Kiểm tra
sudo ./svc.sh status
```

**Bước 5 — Cài dependency trên Pi:**

```bash
sudo apt update
sudo apt install -y stlink-tools sigrok-cli
pip3 install pyocd --break-system-packages
```

**Bước 6 — Cấp quyền USB + GPIO cho user pi:**

```bash
sudo usermod -aG gpio,plugdev pi
# Logout rồi login lại để áp dụng
```

---

### Workflow — `.github/workflows/autograder.yml`

```yaml
name: Autograder

on:
  push:
    paths:
      - 'lab*/main.c'

jobs:
  build-and-grade:
    runs-on: self-hosted
    steps:
      - name: Checkout code
        uses: actions/checkout@v4
        with:
          fetch-depth: 0

      - name: Detect changed lab and run grader
        run: |
          LAB_FOLDER=$(git diff --name-only ${{ github.event.before }} ${{ github.sha }} \
            | grep "^lab.*/main\.c$" \
            | cut -d'/' -f1 \
            | sort -u | head -1)

          echo "Lab detected: $LAB_FOLDER"

          if [ -z "$LAB_FOLDER" ]; then
            echo "Khong phat hien lab nao thay doi, bo qua."
            exit 0
          fi

          python3 /home/pi/ve_lab/logic/main_grader.py --lab "$LAB_FOLDER" 2>&1 \
            | tee -a /home/pi/ve_lab/autograder.log

```

**Lưu ý detect lab:**
- Dùng `github.event.before` thay vì `HEAD~1` — chính xác hơn khi sinh viên push nhiều commit một lúc
- `before = 000...0` nghĩa là push lần đầu (repo mới) → dùng `git show` thay vì `git diff`

---

## Pipeline chấm điểm — `main_grader.py`

```
GitHub push → Actions Runner trigger
        ↓
main_grader.py --lab labX_name
        ↓
1. gpio_setup()
   Export GPIO NRST pins qua sysfs, set HIGH (chạy bình thường)
        ↓
2. build(lab_folder)
   Lấy main.c từ $GITHUB_WORKSPACE/labX_name/main.c
   Copy → stm32_temp_stu/Src/main.c
   make clean && make → build/template_make.hex
        ↓
3. flash(simulator.hex, STLINK_SIM)
   Flash firmware simulator nếu có trong assignment/labX_name/
        ↓
4. flash(template_make.hex, STLINK_STUDENT)
   Flash firmware sinh viên vào Student STM32
        ↓
5. load_lab_json(lab_folder)
   Đọc labX.json → cfg (dùng cho cả check_reg lẫn decoders)
        ↓
6. release_reset() → sleep(0.3)
   Thả NRST cả 2 STM32 → firmware init chạy (SystemInit + peripheral init)
   sleep 300ms đủ để toàn bộ init code hoàn thành
        ↓
7. check_reg.run()                          ← firmware đã init xong
   pyOCD kết nối STLink Student qua SWD
   Halt CPU → đọc thanh ghi → resume CPU
   So sánh với expected trong JSON → reg_results[]
        ↓
8. hold_reset() → xóa result.csv cũ
   Kéo NRST cả 2 STM32 xuống LOW — chip dừng hoàn toàn
   Xóa CSV cũ để capture sạch
        ↓
9. load_lab_decoders(lab_folder) + sigrok.start_capture()   ← NON-BLOCKING
   Spawn sigrok-cli process + process_stdout thread
   sigrok đang init USB, chưa có tín hiệu
        ↓
10. sleep(0.5)
    Đợi sigrok init xong, sẵn sàng sample
        ↓
11. release_reset()
    Thả NRST cả 2 STM32 cùng lúc → chip bắt đầu chạy
    sigrok đã sẵn sàng → capture được từ byte tín hiệu đầu tiên
        ↓
12. sigrok.wait_capture()                   ← BLOCKING đợi hết capture_time
    Đợi sigrok-cli process kết thúc + process_stdout thread flush CSV
    → result.csv đầy đủ từ đầu đến cuối
        ↓
13. check_logic.grade()
    Đọc result.csv, chấm theo test_cases trong JSON
    → logic_result{}
        ↓
14. Tổng hợp reg_results + logic_result → in kết quả
    (hiển thị trên GitHub Actions)
```

**Output cuối trên GitHub Actions:**
```
Logic : 60/70
Reg   : 20/30
TONG  : 80/100 (80%) — PASS
```

---

## Capture & Decode — `sigrok.py`

Spawn `sigrok-cli` process qua `subprocess.Popen`, dùng thread riêng đọc stdout và parse.

**Kiến trúc — 2 process, 2 thread:**

```
OS:
├── Process: python3 main_grader.py
│   ├── Thread: main thread        → điều phối flow, release_reset, wait
│   └── Thread: process_stdout     → đọc pipe từ sigrok, parse, ghi CSV
│
└── Process: sigrok-cli            → sample USB (2MHz) → decode → stdout
```

Hai process giao tiếp qua **pipe** (OS buffer một chiều). sigrok-cli write annotation ra stdout, `process_stdout` thread đọc liên tục từ pipe đầu kia.

**3 hàm public:**

| Hàm | Blocking | Dùng khi |
|---|---|---|
| `start_capture(decoders, time)` | Không | Gọi trước `release_reset()` — sigrok khởi động trước |
| `wait_capture()` | Có | Gọi sau `release_reset()` — đợi capture xong + CSV flush |
| `run_capture(decoders, time)` | Có | Gọi standalone trực tiếp từ CLI |

**Output:**
- In bảng ra terminal
- Ghi `logic/result.csv`

**Decoders** load từ `labX.json["decoders"]`. Nếu không có → dùng `DEFAULT_DECODERS` (tất cả protocol).

**DHT11** được decode thủ công qua `decode_dht11()` — parse timing pulses từ sigrok `timing` decoder.

---

## Chấm điểm logic — `check_logic.py`

Đọc `result.csv` sau khi sigrok capture xong, chấm theo `test_cases` trong `labX.json`. Chỉ trả về kết quả logic — không gọi `check_reg`.

**5 loại check:**

| Type | Mô tả |
|---|---|
| `contains` | Tìm chuỗi con trong values |
| `regex` | Match pattern, có `min_count` |
| `regex_value_range` | Extract số từ regex, kiểm tra trong `[min, max]` |
| `i2c_addr_ack` | Kiểm tra địa chỉ I2C được ACK (cả read lẫn write) |
| `dht11_checksum` | Parse 5 byte hex, tính và verify checksum |

---

## Kiểm tra thanh ghi — `check_reg.py`

Dùng **pyOCD** kết nối STLink Student qua SWD, **halt CPU**, đọc từng địa chỉ thanh ghi, apply mask, so sánh với expected.

Được gọi từ `main_grader.py` **sau** `release_reset() + sleep(0.3)` — đảm bảo firmware init đã chạy xong, thanh ghi peripheral có giá trị hợp lệ. pyOCD tự halt CPU trước khi đọc và resume sau khi xong.

> **Lưu ý:** Không đọc thanh ghi khi chip đang hold NRST — lúc đó toàn bộ peripheral bus chưa power-on, mọi địa chỉ trả về 0x00000000.

Dùng `finally` để `t.resume()` luôn chạy dù có exception ở bước nào — tránh CPU bị kẹt halt.

```json
{
  "id": "REG01",
  "name": "Kiem tra GPIO config",
  "address": "0x40011000",
  "mask": "0xFF",
  "expected": "0x01",
  "points": 10,
  "comment": "GPIOA CRL"
}
```

Có thể chạy **standalone** để debug:
```bash
python3 /home/pi/ve_lab/logic/check_reg.py --lab lab5_i2c
python3 /home/pi/ve_lab/logic/check_reg.py --lab lab5_i2c --serial 37FF71064E573436DB011543
```

---

## Cấu trúc `labX.json`

```json
{
  "target": "stm32f103c8",
  "decoders": [
    "i2c:scl=D0:sda=D1",
    "uart:baudrate=115200:tx=D2:format=hex"
  ],
  "test_cases": [
    {
      "id": "TC01",
      "name": "Kiem tra dia chi I2C",
      "points": 30,
      "verify": {
        "source": "i2c",
        "type": "i2c_addr_ack",
        "addr": "48"
      }
    }
  ],
  "register_checks": [
    {
      "id": "REG01",
      "name": "Kiem tra GPIO config",
      "address": "0x40011000",
      "mask": "0xFF",
      "expected": "0x01",
      "points": 10,
      "comment": "GPIOA CRL"
    }
  ]
}
```

---

## Bugs / TODO

| # | Vấn đề | File | Mức độ |
|---|---|---|---|
| 1 | Switch Matrix CH446Q chưa có code implement | — | ⬜ Phase 2 |

---

## Các lệnh hữu ích

```bash
# Chạy thủ công
python3 /home/pi/ve_lab/logic/main_grader.py --lab lab5_i2c
python3 /home/pi/ve_lab/logic/main_grader.py --lab lab1_gpio --time 30

# Chấm từ CSV đã capture sẵn
python3 /home/pi/ve_lab/logic/check_logic.py --lab lab5_i2c
python3 /home/pi/ve_lab/logic/check_logic.py --lab lab5_i2c --csv /path/to/result.csv

# Kiểm tra thanh ghi độc lập
python3 /home/pi/ve_lab/logic/check_reg.py --lab lab5_i2c

# Capture tín hiệu thủ công
python3 /home/pi/ve_lab/logic/sigrok.py --lab lab5_i2c --time 20

# Kiểm tra runner
sudo ./svc.sh status   # trong ~/actions-runner
```