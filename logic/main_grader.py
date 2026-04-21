#!/usr/bin/env python3
"""
Embedded Hardware Autograder — Main Grader
Orchestrate toan bo luong: build → flash → capture → grade → result

Su dung:
  python3 main_grader.py --lab lab1-gpio
  python3 main_grader.py --lab lab5-i2c --time 20
"""

import os
import re
import sys
import argparse
import subprocess
import time
from datetime import datetime

import json
import sigrok
import check_logic
import check_reg

# ─── Config ───────────────────────────────────────────────────────────────────

BASE_DIR        = "/home/pi/ve_lab"
TEMPLATE_DIR    = f"{BASE_DIR}/stm32_temp_stu"
ASSIGNMENTS_DIR = f"{BASE_DIR}/assignment"
LOGIC_DIR       = f"{BASE_DIR}/logic"

STLINK_STUDENT  = "37FF71064E573436DB011543"
STLINK_SIM      = "10002D00182D343632525544"

GPIO_STUDENT    = 529   # NRST STM32 Student
GPIO_SIM        = 539   # NRST STM32 Simulator

CAPTURE_TIME    = 20
REPO_DIR        = os.environ.get("GITHUB_WORKSPACE", BASE_DIR)
CSV_PATH        = f"{LOGIC_DIR}/result.csv"

# ─── Log ──────────────────────────────────────────────────────────────────────

def log(msg):
    print(f"[{datetime.now().strftime('%H:%M:%S')}] {msg}", flush=True)

# ─── GPIO ─────────────────────────────────────────────────────────────────────

def gpio_setup():
    log("Setup GPIO NRST...")
    for pin in [GPIO_STUDENT, GPIO_SIM]:
        if not os.path.exists(f"/sys/class/gpio/gpio{pin}"):
            with open("/sys/class/gpio/export", "w") as f:
                f.write(str(pin))
        with open(f"/sys/class/gpio/gpio{pin}/direction", "w") as f:
            f.write("out")
        with open(f"/sys/class/gpio/gpio{pin}/value", "w") as f:
            f.write("1")  # HIGH = chay binh thuong
    log("OK: GPIO san sang")


def gpio_write(pin, value):
    with open(f"/sys/class/gpio/gpio{pin}/value", "w") as f:
        f.write(str(value))


def hold_reset():
    gpio_write(GPIO_STUDENT, 0)
    gpio_write(GPIO_SIM, 0)
    log("OK: Ca 2 STM32 dang bi giu reset")


def release_reset():
    gpio_write(GPIO_STUDENT, 1)
    gpio_write(GPIO_SIM, 1)
    log("OK: Ca 2 STM32 da duoc nha, dang chay")

# ─── Build ────────────────────────────────────────────────────────────────────

def build(lab_folder, repo_dir):
    """Copy main.c tu runner workspace → template → make."""
    # Runner checkout code vao _work/, lay tu bien moi truong GITHUB_WORKSPACE
    workspace = os.environ.get("GITHUB_WORKSPACE", repo_dir)
    src = os.path.join(workspace, lab_folder, "main.c")

    if not os.path.exists(src):
        log(f"LOI: Khong tim thay {src}")
        return False

    dst = os.path.join(TEMPLATE_DIR, "Src", "main.c")
    log(f"Copy {src} → {dst}")
    subprocess.run(["cp", src, dst], check=True)

    log("Building...")
    result = subprocess.run(
        ["make", "clean"],
        cwd=TEMPLATE_DIR
    )
    result = subprocess.run(
        ["make"],
        cwd=TEMPLATE_DIR
    )
    if result.returncode != 0:
        log(f"LOI: Build FAILED [{lab_folder}]")
        return False

    log(f"OK: Build thanh cong [{lab_folder}]")
    return True

# ─── Flash ────────────────────────────────────────────────────────────────────

def flash(hex_path, serial, label):
    log(f"Flashing {label}...")
    result = subprocess.run([
        "st-flash", "--serial", serial,
        "--format=ihex", "write", hex_path
    ])
    if result.returncode != 0:
        log(f"LOI: Flash FAILED [{label}]")
        return False
    log(f"OK: Flash thanh cong [{label}]")
    return True



# ─── Main ─────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="Autograder Main Grader")
    parser.add_argument("--lab",  required=True, help="Ten lab (vd: lab1-gpio)")
    parser.add_argument("--time", type=int, default=CAPTURE_TIME,
                        help=f"Thoi gian capture (giay), mac dinh: {CAPTURE_TIME}")
    args = parser.parse_args()

    lab_folder   = args.lab
    capture_time = args.time

    log(f"{'='*60}")
    log(f"MAIN GRADER: {lab_folder}")
    log(f"{'='*60}")

    # ── Resolve JSON path ─────────────────────────────────────────────────────
    lab_num = re.search(r"lab(\d+)", lab_folder)
    if not lab_num:
        log(f"LOI: Khong xac dinh duoc lab number tu: {lab_folder}")
        sys.exit(1)
    json_path = os.path.join(ASSIGNMENTS_DIR, lab_folder, f"lab{lab_num.group(1)}.json")

    # ── Buoc 1: Setup GPIO ────────────────────────────────────────────────────
    gpio_setup()

    # ── Buoc 2: Build ─────────────────────────────────────────────────────────
    if not build(lab_folder, REPO_DIR):
        sys.exit(1)

    # ── Buoc 3: Flash Simulator ───────────────────────────────────────────────
    sim_hex = os.path.join(ASSIGNMENTS_DIR, lab_folder, "simulator.hex")
    if os.path.exists(sim_hex):
        if not flash(sim_hex, STLINK_SIM, "SIMULATOR"):
            log(f"WARN: Flash simulator that bai, bo qua")
    else:
        log(f"WARN: Khong co simulator.hex cho {lab_folder}, bo qua")

    # ── Buoc 4: Flash Student ─────────────────────────────────────────────────
    student_hex = os.path.join(TEMPLATE_DIR, "build", "template_make.hex")
    if not flash(student_hex, STLINK_STUDENT, "STUDENT"):
        sys.exit(1)

    # ── Buoc 5: Load JSON config ──────────────────────────────────────────────
    with open(json_path, encoding="utf-8") as f:
        cfg = json.load(f)

    # ── Buoc 6: Release reset → cho init code chay ───────────────────────────
    release_reset()
    time.sleep(0.3)  # du de SystemInit + peripheral init chay xong

    # ── Buoc 7: Check register (init da chay, pyOCD halt → doc → resume) ─────
    reg_results = []
    reg_checks  = cfg.get("register_checks", [])
    if reg_checks:
        target = cfg.get("target", "stm32f103c8")
        reg_results = check_reg.run(reg_checks, target=target, serial=STLINK_STUDENT)
    else:
        log("Khong co register_checks trong JSON, bo qua")

    # ── Buoc 8: Hold reset, xoa CSV cu, khoi dong sigrok san sang ─────────────
    hold_reset()
    if os.path.exists(CSV_PATH):
        os.remove(CSV_PATH)
        log("Da xoa result.csv cu")

    # ── Buoc 9: Load decoders, khoi dong sigrok (non-blocking) ───────────────
    decoders = sigrok.load_lab_decoders(lab_folder)

    log(f"Bat dau capture {capture_time}s...")
    sigrok.start_capture(decoders, capture_time=capture_time)  # non-blocking

    time.sleep(0.5)  # doi sigrok init xong, san sang sample

    # ── Buoc 10: Nha reset → ca 2 STM32 start dong thoi, sigrok bat tu dau ───
    release_reset()

    sigrok.wait_capture()  # blocking doi den khi sigrok ghi xong result.csv

    # ── Buoc 11: Cham diem logic tu result.csv ────────────────────────────────
    log("Bat dau cham diem logic...")
    logic_result = check_logic.grade(lab_folder, json_path, csv_path=CSV_PATH)

    if logic_result is None:
        log("LOI: check_logic khong tra ve ket qua")
        sys.exit(1)

    # ── Buoc 12: Tong hop ket qua reg + logic ────────────────────────────────
    all_results  = logic_result["test_cases"] + reg_results
    total_earned = logic_result["score"] + sum(r["score"] for r in reg_results)
    total_max    = logic_result["max"]   + sum(r["max"]   for r in reg_results)
    percent      = round(total_earned / total_max * 100) if total_max else 0
    overall      = "PASS" if percent >= 50 else "FAIL"

    log(f"{'='*60}")
    log(f"KET QUA TONG HOP: {lab_folder}")
    log(f"  Logic : {logic_result['score']}/{logic_result['max']}")
    log(f"  Reg   : {sum(r['score'] for r in reg_results)}/{sum(r['max'] for r in reg_results)}")
    log(f"  TONG  : {total_earned}/{total_max} ({percent}%) — {overall}")
    log(f"{'='*60}")


if __name__ == "__main__":
    main()