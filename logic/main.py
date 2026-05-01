#!/usr/bin/env python3
"""
EMBEDDED HARDWARE AUTOGRADER - MAIN ORCHESTRATOR
------------------------------------------------
Tac gia: Antigravity AI & Team
Mo ta: Dieu phoi toan bo quy trinh tu Build, Flash den Capture va Cham diem.
Luong thuc thi:
  1. Setup He thong (GPIO, Paths)
  2. Build Firmware (Student & Simulator)
  3. Flash Firmware xuong MCUs
  4. Khoi chay va Kiem tra Register (Init phase)
  5. Capture tin hieu Logic (Execution phase)
  6. Tong hop ket qua và Cham diem
"""

import os
import re
import sys
import time
import json
import argparse
import subprocess
from datetime import datetime

# Import cac module chuc nang
import builder
import sigrok
import check_logic
import check_reg

# ==============================================================================
# SECTION 1: CONFIGURATION & PATHS
# ==============================================================================
BASE_DIR        = "/home/pi/ve_lab"
TEMPLATE_DIR    = f"{BASE_DIR}/stm32_temp_stu"
ASSIGNMENTS_DIR = f"{BASE_DIR}/assignment"
LOGIC_DIR       = f"{BASE_DIR}/logic"

# ST-Link Serials cho tung thiet bi
STLINK_STUDENT  = "37FF71064E573436DB011543"
STLINK_SIM      = "10002D00182D343632525544"

# GPIO Pins dieu khien chan NRST (Reset) cua STM32
GPIO_STUDENT    = 529   # NRST STM32 Student
GPIO_SIM        = 539   # NRST STM32 Simulator

# Thong so mac dinh
CAPTURE_TIME    = 20
REPO_DIR        = os.environ.get("GITHUB_WORKSPACE", BASE_DIR)
CSV_PATH        = f"{LOGIC_DIR}/result.csv"

# ==============================================================================
# SECTION 2: UTILITIES & SYSTEM CONTROL
# ==============================================================================

def log(msg):
    """Ghi log kem thoi gian thuc."""
    print(f"[{datetime.now().strftime('%H:%M:%S')}] {msg}", flush=True)

def gpio_control(action):
    """Dieu khien trang thai Reset cua ca 2 MCU."""
    for pin in [GPIO_STUDENT, GPIO_SIM]:
        path = f"/sys/class/gpio/gpio{pin}/value"
        if not os.path.exists(path): continue

        with open(path, "w") as f:
            if action == "HOLD":
                f.write("0") # Giu Reset (Active Low)
            else:
                f.write("1") # Nha Reset

    status = "dang bi giu RESET" if action == "HOLD" else "da duoc NHA (dang chay)"
    log(f"He thong {status}")

def setup_hardware():
    """Khoi tao cac chan GPIO tren Raspberry Pi."""
    log("Dang khoi tao phan cung (GPIO NRST)...")
    for pin in [GPIO_STUDENT, GPIO_SIM]:
        if not os.path.exists(f"/sys/class/gpio/gpio{pin}"):
            with open("/sys/class/gpio/export", "w") as f:
                f.write(str(pin))
        with open(f"/sys/class/gpio/gpio{pin}/direction", "w") as f:
            f.write("out")
    gpio_control("RELEASE") # Mac dinh cho phep chay

# ==============================================================================
# SECTION 3: DEPLOYMENT LOGIC (FLASHING)
# ==============================================================================

def flash_firmware(hex_path, serial, label):
    """Nap file hex xuong MCU qua ST-Link."""
    log(f"Dang nap firmware cho {label}...")
    try:
        result = subprocess.run([
            "st-flash", "--serial", serial,
            "--format=ihex", "write", hex_path
        ], stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)

        if result.returncode == 0:
            log(f"OK: Nap {label} thanh cong")
            return True
        else:
            log(f"LOI: Nap {label} that bai")
            return False
    except Exception as e:
        log(f"LOI: He thong nap gap su co: {e}")
        return False

# ==============================================================================
# SECTION 4: MAIN PIPELINE
# ==============================================================================

def run_autograder(args):
    lab_name = args.lab
    cap_time = args.time

    # Kiem tra xem co dang chay che do manual hay khong
    manual_mode = any([args.build, args.flash, args.reg, args.capture, args.grade])

    # 1. Resolve Lab Config
    lab_dir = os.path.join(ASSIGNMENTS_DIR, lab_name)
    if not os.path.exists(lab_dir):
        log(f"LOI: Thu muc lab khong ton tai: {lab_dir}")
        return

    json_files = [f for f in os.listdir(lab_dir) if f.endswith(".json")]
    if not json_files:
        log(f"LOI: Khong tim thay file .json trong {lab_dir}")
        return
    json_path = os.path.join(lab_dir, json_files[0])
    log(f"Su dung cau hinh: {os.path.basename(json_path)}")

    # 2. Build Phase
    if not manual_mode or args.build:
        log("--- PHASE 1: BUILDING ---")
        if not builder.build_student(lab_name, REPO_DIR): return
        if not builder.build_simulator(lab_name): return

    # 3. Flash Phase
    if not manual_mode or args.flash:
        log("--- PHASE 2: FLASHING ---")
        setup_hardware()

        sim_hex = "/home/pi/ve_lab/stm32_temp_sim/build/sim.hex"
        if not flash_firmware(sim_hex, STLINK_SIM, "SIMULATOR"): return

        stu_hex = "/home/pi/ve_lab/stm32_temp_stu/build/stu.hex"
        if not flash_firmware(stu_hex, STLINK_STUDENT, "STUDENT"): return

    # 4. Init & Register Check Phase
    if not manual_mode or args.reg:
        log("--- PHASE 3: REGISTER CHECK ---")
        with open(json_path, encoding="utf-8") as f:
            cfg = json.load(f)

        gpio_control("RELEASE")
        time.sleep(1.5)
        if cfg.get("register_checks"):
            check_reg.run(cfg["register_checks"], serial=STLINK_STUDENT)

    # 5. Capture Logic Phase
    if not manual_mode or args.capture:
        log("--- PHASE 4: LOGIC CAPTURE ---")
        setup_hardware()
        gpio_control("HOLD")
        if os.path.exists(CSV_PATH): os.remove(CSV_PATH)

        decoders = sigrok.load_lab_decoders(lab_name)
        sigrok.start_capture(decoders, capture_time=cap_time)
        time.sleep(1.5)

        gpio_control("RELEASE")
        sigrok.wait_capture()

    # 6. Grading Phase
    if not manual_mode or args.grade:
        log("--- PHASE 5: GRADING ---")
        check_logic.grade(lab_name, json_path, csv_path=CSV_PATH)
        log("Hoan tat qua trinh cham diem.")

# ==============================================================================
# SECTION 5: ENTRY POINT
# ==============================================================================

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Autograder Main Entry")
    parser.add_argument("--lab",     required=True, help="Ten thu muc lab")
    parser.add_argument("--time",    type=int, default=CAPTURE_TIME, help="Thoi gian capture")

    # Manual Testing Flags
    parser.add_argument("--build",   action="store_true", help="Chi thuc hien Build")
    parser.add_argument("--flash",   action="store_true", help="Chi thuc hien Flash")
    parser.add_argument("--reg",     action="store_true", help="Chi thuc hien Register Check")
    parser.add_argument("--capture", action="store_true", help="Chi thuc hien Capture")
    parser.add_argument("--grade",   action="store_true", help="Chi thuc hien Grade")

    args = parser.parse_args()

    run_autograder(args)
