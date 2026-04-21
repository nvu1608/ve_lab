#!/usr/bin/env python3
"""
Embedded Hardware Autograder — Logic Analyzer Tool
Capture va decode tin hieu tu 8 kenh, hien thi dang bang + luu CSV.

Kenh mac dinh:
  D0 = I2C SCL
  D1 = I2C SDA
  D2 = UART TX
  D3 = DHT11
  D4 = SPI MOSI
  D5 = SPI CLK
  D6 = SPI CS
  D7 = PWM

Su dung:
  python3 sigrok.py                  # kiem tra device + chay
  python3 sigrok.py --no-check       # bo qua kiem tra device (main_grader goi)
  python3 sigrok.py --time 10        # capture 10 giay (mac dinh: 30)
  python3 sigrok.py --lab lab7-i2c   # chi decode protocol cua lab do (tu json)
"""

import subprocess
import re
import time
import sys
import threading
import csv
import json
import os
import argparse
from itertools import zip_longest
from datetime import datetime

# ─── Cau hinh kenh ───────────────────────────────────────────────────────────

CHANNEL_MAP = {
    "i2c_scl":  "D0",
    "i2c_sda":  "D1",
    "uart_tx":  "D2",
    "dht11":    "D3",
    "spi_mosi": "D4",
    "spi_clk":  "D5",
    "spi_cs":   "D6",
    "pwm":      "D7",
}

# ─── Cau hinh decoder theo lab ───────────────────────────────────────────────

DEFAULT_DECODERS = [
    f"i2c:scl={CHANNEL_MAP['i2c_scl']}:sda={CHANNEL_MAP['i2c_sda']}",
    f"uart:baudrate=115200:tx={CHANNEL_MAP['uart_tx']}:format=hex",
    f"pwm:data={CHANNEL_MAP['pwm']}",
    f"spi:mosi={CHANNEL_MAP['spi_mosi']}:clk={CHANNEL_MAP['spi_clk']}:cs={CHANNEL_MAP['spi_cs']}:wordsize=8:cpol=0:cpha=0",
    f"timing:data={CHANNEL_MAP['i2c_scl']}",
    f"timing:data={CHANNEL_MAP['spi_clk']}",
    f"timing:data={CHANNEL_MAP['dht11']}",
]

ASSIGNMENTS_DIR = "/home/pi/ve_lab/assignment"
CSV_PATH        = "/home/pi/ve_lab/logic/result.csv"
SAMPLERATE      = "2M"

# ─── Header ──────────────────────────────────────────────────────────────────

COL = {
    "time":     11,
    "i2c_clk":  9,
    "i2c":      14,
    "uart":     24,
    "pwm":      14,
    "dht11":    19,
    "spi_clk":  9,
    "spi":      12,
    "cs":       8,
}

def print_header():
    h = (
        f"{'THOI GIAN':<{COL['time']}} | "
        f"{'I2C CLK':<{COL['i2c_clk']}} | "
        f"{'I2C':<{COL['i2c']}} | "
        f"{'UART':<{COL['uart']}} | "
        f"{'PWM':<{COL['pwm']}} | "
        f"{'DHT11':<{COL['dht11']}} | "
        f"{'SPI CLK':<{COL['spi_clk']}} | "
        f"{'SPI':<{COL['spi']}} | "
        f"{'CS':<{COL['cs']}}"
    )
    sep = "-" * len(h)
    print("=" * len(h))
    print(h)
    print(sep)
    return sep

def print_row(ts, i2c_clk="", i2c="", uart="", pwm="", dht11="", spi_clk="", spi="", cs=""):
    print(
        f"[{ts:>{COL['time']-2}}] | "
        f"{i2c_clk:<{COL['i2c_clk']}} | "
        f"{i2c:<{COL['i2c']}} | "
        f"{uart:<{COL['uart']}} | "
        f"{pwm:<{COL['pwm']}} | "
        f"{dht11:<{COL['dht11']}} | "
        f"{spi_clk:<{COL['spi_clk']}} | "
        f"{spi:<{COL['spi']}} | "
        f"{cs:<{COL['cs']}}"
    )

# ─── DHT11 decoder ───────────────────────────────────────────────────────────

def decode_dht11(pulses_us):
    """
    pulses_us: list do rong tung pulse xen ke LOW/HIGH
    Pattern thuc te:
      [0] 20ms   = start LOW
      [1] ~33us  = response LOW
      [2] ~62us  = response HIGH
      [3] ~81us  = sync (co the co hoac khong)
      [4+] cap (LOW~52us, HIGH~27us=bit0 | ~71us=bit1)
    """
    if len(pulses_us) < 43:
        return None

    idx = 3
    bits = []
    while idx < len(pulses_us) - 1 and len(bits) < 40:
        low_p  = pulses_us[idx]
        high_p = pulses_us[idx + 1]
        if 35 < low_p < 75:
            bits.append(1 if high_p > 42 else 0)
            idx += 2
        else:
            idx += 1

    if len(bits) < 40:
        return None

    data = [0] * 5
    for i in range(40):
        if bits[i]:
            data[i // 8] |= (1 << (7 - (i % 8)))

    checksum = sum(data[:4]) & 0xFF
    if checksum != data[4]:
        return None

    return f"{data[0]:02X} {data[1]:02X} {data[2]:02X} {data[3]:02X} {data[4]:02X}"

# ─── Kiem tra device ─────────────────────────────────────────────────────────

def check_device():
    print("=" * 80)
    print(" [HE THONG] Kiem tra Logic Analyzer...")
    result = subprocess.run(
        ["sigrok-cli", "-d", "fx2lafw", "--scan"],
        capture_output=True, text=True, timeout=10
    )
    combined = result.stdout + result.stderr
    if "fx2lafw" in combined.lower() or "saleae" in combined.lower():
        print(" OK: Logic Analyzer san sang!")
        return True
    print(" LOI: Khong tim thay Logic Analyzer!")
    print(f"  stdout: {result.stdout.strip()}")
    print(f"  stderr: {result.stderr.strip()}")
    return False

# ─── Doc cau hinh lab tu JSON ─────────────────────────────────────────────────

def load_lab_decoders(lab_name):
    """
    Doc file autograder-assignments/<lab_name>/labX.json
    Tra ve list decoder string, hoac DEFAULT_DECODERS neu khong co file.
    """
    lab_num = re.search(r'lab(\d+)', lab_name)
    if not lab_num:
        return DEFAULT_DECODERS

    json_path = os.path.join(ASSIGNMENTS_DIR, lab_name, f"lab{lab_num.group(1)}.json")
    if not os.path.exists(json_path):
        print(f" [WARN] Khong tim thay {json_path}, dung DEFAULT_DECODERS")
        return DEFAULT_DECODERS

    with open(json_path) as f:
        cfg = json.load(f)

    decoders = cfg.get("decoders", [])
    if not decoders:
        return DEFAULT_DECODERS

    decoder_str = ",".join(decoders)
    if "i2c" in decoder_str:
        decoders.append(f"timing:data={CHANNEL_MAP['i2c_scl']}")
    if "spi" in decoder_str:
        decoders.append(f"timing:data={CHANNEL_MAP['spi_clk']}")
    dht_decoder = f"timing:data={CHANNEL_MAP['dht11']}"
    if ("dht11" in decoder_str or "timing" in decoder_str) and dht_decoder not in decoders:
        decoders.append(dht_decoder)

    print(f" [LAB] Load {json_path}")
    print(f" [DECODER] {', '.join(decoders)}")
    return decoders

# ─── Main capture ─────────────────────────────────────────────────────────────

_proc   = None   # sigrok-cli process
_thread = None   # thread doc stdout

def start_capture(decoders, capture_time=30):
    """Khoi dong sigrok-cli NON-BLOCKING. Goi truoc khi nha reset."""
    global _proc, _thread
    _proc, _thread = _run_capture_impl(decoders, capture_time)

def wait_capture():
    """Doi sigrok-cli va thread xu ly stdout ket thuc."""
    global _proc, _thread
    if _proc:
        _proc.wait()
    if _thread:
        _thread.join()
    _proc = None
    _thread = None

def run_capture(decoders, capture_time=30):
    """Blocking — dung khi goi standalone."""
    proc, thread = _run_capture_impl(decoders, capture_time)
    proc.wait()
    thread.join()

def _run_capture_impl(decoders, capture_time=30):

    decoder_str = ",".join(decoders)

    annots = []
    if "i2c" in decoder_str:
        annots.append("i2c=address-write:address-read:data-write:data-read")
    if "uart" in decoder_str:
        annots.append("uart=tx-data")
    if "pwm" in decoder_str:
        annots.append("pwm=duty-cycle:period")
    if "spi" in decoder_str:
        annots.append("spi=mosi-data")
    if "timing" in decoder_str:
        annots.append("timing=time")

    cmd = [
        "sigrok-cli",
        "-d", "fx2lafw",
        "--config", f"samplerate={SAMPLERATE}",
        "--time", f"{capture_time}s",
        "--channels", "D0,D1,D2,D3,D4,D5,D6,D7",
        *[arg for d in decoders for arg in ("-P", d)],
        "-A", ",".join(annots),
    ]

    print(f"\n [CMD] {' '.join(cmd)}\n")

    timing_channels = []
    for d in decoders:
        m = re.search(r'timing:data=(D\d+)', d)
        if m:
            timing_channels.append(m.group(1))

    # CSV
    os.makedirs(os.path.dirname(CSV_PATH), exist_ok=True)
    f_csv = open(CSV_PATH, "w", newline="", encoding="utf-8")
    writer = csv.writer(f_csv)
    writer.writerow(["Time(ms)", "I2C_CLK", "I2C", "UART", "PWM", "DHT11", "SPI_CLK", "SPI", "CS"])
    f_csv.flush()

    sep = print_header()

    # State
    st = {
        "base_time":    None,
        "freq_i2c":     "--",
        "freq_spi":     "--",
        "uart_buf":     "",
        "uart_last_t":  0,
        "pwm_last":     "",
        "pwm_time":     0,
        "dht_pulses":   [],
        "dht_last":     "",
        "dht_last_t":   0,
        "cs_active":    False,
        "spi_last_t":   0,
        "timing_half":  {},
        "timing_buf":   {},
    }

    def write_row(ts, **kwargs):
        """In ra terminal va ghi CSV."""
        cols = {
            "i2c_clk": "", "i2c": "", "uart": "",
            "pwm": "", "dht11": "", "spi_clk": "", "spi": "", "cs": ""
        }
        cols.update(kwargs)
        print_row(ts, **cols)
        writer.writerow([
            ts, cols["i2c_clk"], cols["i2c"], cols["uart"],
            cols["pwm"], cols["dht11"], cols["spi_clk"], cols["spi"], cols["cs"]
        ])
        f_csv.flush()

    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1,          # line-buffered
    )

    def drain_stderr():
        for line in iter(proc.stderr.readline, ""):
            line = line.strip()
            if line and "warning" not in line.lower() and "libusb" not in line.lower():
                print(f" [SIGROK] {line}", flush=True)
    threading.Thread(target=drain_stderr, daemon=True).start()

    def process_stdout():
        try:
            for raw in iter(proc.stdout.readline, ""):
                raw = raw.strip()
                if not raw:
                    continue
                if any(x in raw.lower() for x in ["warning", "libusb", "note"]):
                    continue

                now = time.perf_counter_ns()
                if st["base_time"] is None:
                    st["base_time"] = now
                ts = f"{(now - st['base_time']) / 1e6:.3f}"

                low = raw.lower()

                # ── I2C ──────────────────────────────────────────────────────
                if "i2c" in low:
                    m = re.search(r'(?:address|data)[- ](?:write|read):\s*([0-9a-fA-F]+)', raw, re.I)
                    if m:
                        is_addr = "address" in low
                        is_write = "write" in low
                        prefix = ("ADDR" if is_addr else "DATA") + ("W:" if is_write else "R:")
                        suffix = "(NACK)" if "nack" in low else ""
                        write_row(ts,
                            i2c_clk=st["freq_i2c"],
                            i2c=prefix + m.group(1).upper() + suffix
                        )

                # ── UART ─────────────────────────────────────────────────────
                elif "uart" in low:
                    m = re.search(r'uart-\d+:\s*([0-9a-fA-F]{2})\s*$', raw, re.I)
                    if m:
                        try:
                            ch = chr(int(m.group(1), 16))
                            st["uart_last_t"] = now
                            if ch in ('\n', '\r'):
                                if st["uart_buf"]:
                                    write_row(ts, uart=st["uart_buf"].strip())
                                    st["uart_buf"] = ""
                            elif ch.isprintable():
                                st["uart_buf"] += ch
                        except ValueError:
                            pass

                # ── SPI ──────────────────────────────────────────────────────
                elif "spi" in low:
                    m = re.search(r'spi-\d+:\s*([0-9a-fA-F]{2})\s*$', raw, re.I)
                    if m:
                        st["spi_last_t"] = now
                        if not st["cs_active"]:
                            write_row(ts, cs="LOW")
                            st["cs_active"] = True
                        write_row(ts,
                            spi_clk=st["freq_spi"],
                            spi="0x" + m.group(1).upper()
                        )

                # ── PWM ──────────────────────────────────────────────────────
                elif "pwm" in low:
                    freq_m = re.search(r'([0-9.]+)\s*([kK]?)Hz', raw, re.I)
                    duty_m = re.search(r'([0-9.]+)\s*%', raw)
                    if freq_m and duty_m:
                        freq_val = float(freq_m.group(1))
                        if freq_m.group(2).lower() != 'k':
                            freq_val /= 1000.0
                        pwm_str = f"{freq_val:.1f}kHz {round(float(duty_m.group(1)), 1)}%"
                        if pwm_str != st["pwm_last"] or (now - st["pwm_time"]) > 1_000_000_000:
                            st["pwm_last"] = pwm_str
                            st["pwm_time"] = now
                            write_row(ts, pwm=pwm_str)

                # ── TIMING ───────────────────────────────────────────────────
                elif "timing" in low:
                    ch_m = re.search(r'timing-(\d+)', raw)
                    ch_id = int(ch_m.group(1)) if ch_m else 0
                    ch_name = timing_channels[ch_id - 1] if 0 < ch_id <= len(timing_channels) else None

                    freq_m = re.search(r'\(([0-9.]+)\s*([kKmM]?)Hz\)', raw)
                    if freq_m and ch_name == CHANNEL_MAP['i2c_scl']:
                        fval = float(freq_m.group(1))
                        funit = freq_m.group(2).lower()
                        if funit == 'm':
                            fval *= 1000.0
                        elif funit == '':
                            fval /= 1000.0
                        if fval > 50:
                            st["freq_i2c"] = f"{fval:.1f}k"

                    elif freq_m and ch_name == CHANNEL_MAP['spi_clk']:
                        fval = float(freq_m.group(1))
                        funit = freq_m.group(2).lower()
                        if funit == 'm':
                            fval *= 1000.0
                        elif funit == '':
                            fval /= 1000.0
                        if fval > 50:
                            st["freq_spi"] = f"{fval:.1f}k"

                    elif ch_name == CHANNEL_MAP['dht11']:
                        time_m = re.search(r'([0-9.]+)\s*([μµum]s|ms|ns|s)\b', raw, re.I)
                        if time_m:
                            val = float(time_m.group(1))
                            unit = time_m.group(2).lower().replace('μ','u').replace('µ','u')
                            to_us = {"s": 1e6, "ms": 1e3, "us": 1.0, "ns": 1e-3}
                            us = val * to_us.get(unit, 1.0)
                            if ch_id not in st["timing_buf"]:
                                st["timing_buf"][ch_id] = []

                            if 12000 < us < 30000:
                                buf = st["timing_buf"][ch_id]
                                if len(buf) >= 40:
                                    result = decode_dht11(buf)
                                    if result:
                                        if result != st["dht_last"] or (now - st["dht_last_t"]) > 1_000_000_000:
                                            st["dht_last"] = result
                                            st["dht_last_t"] = now
                                            write_row(ts, dht11=result)
                                st["timing_buf"][ch_id] = []
                            elif us < 500000:
                                st["timing_buf"][ch_id].append(us)

                # ── CS HIGH detect (timeout) ──────────────────────────────────
                if st["cs_active"] and (now - st["spi_last_t"]) > 50_000_000:
                    write_row(ts, cs="HIGH")
                    st["cs_active"] = False

                # ── UART timeout flush ────────────────────────────────────────
                if st["uart_buf"] and (now - st["uart_last_t"]) > 500_000_000:
                    write_row(ts, uart=st["uart_buf"].strip())
                    st["uart_buf"] = ""

        except Exception as e:
            print(f" [ERR] process_stdout: {e}", flush=True)
        finally:
            # Flush UART buffer con lai
            if st["uart_buf"]:
                ts = f"{(time.perf_counter_ns() - st['base_time']) / 1e6:.3f}" if st["base_time"] else "0"
                write_row(ts, uart=st["uart_buf"].strip())
                st["uart_buf"] = ""
            f_csv.close()
            print(f"\n [XONG] Da luu: {CSV_PATH}")
            print(f"        Thoi gian: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")

    t = threading.Thread(target=process_stdout, daemon=False)
    t.start()
    return proc, t

# ─── Entry point ──────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="Autograder Logic Analyzer Tool")
    parser.add_argument("--no-check", action="store_true",
                        help="Bo qua kiem tra device (main_grader goi sau flash)")
    parser.add_argument("--time", type=int, default=30,
                        help="Thoi gian capture (giay), mac dinh: 30")
    parser.add_argument("--lab", type=str, default=None,
                        help="Ten lab de doc decoder tu JSON (vd: lab7-i2c)")
    args = parser.parse_args()

    if not args.no_check:
        if not check_device():
            sys.exit(1)

    if args.lab:
        decoders = load_lab_decoders(args.lab)
    else:
        decoders = DEFAULT_DECODERS
        print(" [INFO] Khong co --lab, su dung tat ca decoder mac dinh")

    run_capture(decoders, capture_time=args.time)

if __name__ == "__main__":
    main()