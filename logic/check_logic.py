#!/usr/bin/env python3
"""
Embedded Hardware Autograder — Check Logic
Doc result.csv + labX.json → cham diem theo test cases

Su dung:
  python3 check_logic.py --lab lab5-i2c          # doc tu result.csv mac dinh
  python3 check_logic.py --lab lab1-gpio
  python3 check_logic.py --lab lab5-i2c --csv /path/to/result.csv
"""

import csv
import json
import re
import os
import argparse
from datetime import datetime

# ─── Config ───────────────────────────────────────────────────────────────────

ASSIGNMENTS_DIR = "/home/pi/ve_lab/assignment"
LOGIC_DIR       = "/home/pi/ve_lab/logic"
CSV_PATH        = "/home/pi/ve_lab/logic/result.csv"
STLINK_STUDENT  = "37FF71064E573436DB011543"

# ─── Row loader ───────────────────────────────────────────────────────────────

def load_csv(csv_path):
    """Doc result.csv → list of dict (normalized keys)."""
    rows = []
    with open(csv_path, newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            normalized = {k.strip().lower().replace("(ms)", "").strip(): v.strip()
                          for k, v in row.items()}
            rows.append(normalized)
    return rows





def get_column_values(rows, source):
    """Lay tat ca gia tri khong rong cua cot 'source' tu rows."""
    col_map = {
        "uart":    "uart",
        "i2c":     "i2c",
        "dht11":   "dht11",
        "pwm":     "pwm",
        "spi":     "spi",
        "i2c_clk": "i2c_clk",
    }
    col = col_map.get(source, source)
    return [r[col] for r in rows if col in r and r[col]]


# ─── Check functions ──────────────────────────────────────────────────────────

def check_contains(values, verify):
    target = verify["value"]
    matched = [v for v in values if target in v]
    if matched:
        return True, f"Tim thay '{target}' ({len(matched)} lan)"
    return False, f"Khong tim thay '{target}'"


def check_regex(values, verify):
    pattern   = verify["pattern"]
    min_count = verify.get("min_count", 1)
    matched   = [v for v in values if re.search(pattern, v)]
    if len(matched) >= min_count:
        return True, f"Match '{pattern}': {len(matched)} lan (can {min_count})"
    return False, f"Match '{pattern}': chi {len(matched)} lan (can {min_count})"


def check_regex_value_range(values, verify):
    extracts  = verify["extracts"]
    min_count = verify.get("min_count", 1)
    results   = []

    for ext in extracts:
        name    = ext["name"]
        pattern = ext["pattern"]
        lo      = ext["min"]
        hi      = ext["max"]

        extracted = []
        for v in values:
            m = re.search(pattern, v)
            if m:
                try:
                    extracted.append(float(m.group(1)))
                except ValueError:
                    pass

        if len(extracted) < min_count:
            results.append((False, f"{name}: chi extract duoc {len(extracted)} gia tri (can {min_count})"))
            continue

        out_of_range = [x for x in extracted if not (lo <= x <= hi)]
        if out_of_range:
            results.append((False, f"{name}: gia tri ngoai nguong {out_of_range[:3]} (phai {lo}–{hi})"))
        else:
            results.append((True, f"{name}: {len(extracted)} gia tri OK trong [{lo}, {hi}]"))

    all_pass = all(r[0] for r in results)
    msg = " | ".join(r[1] for r in results)
    return all_pass, msg


def check_i2c_addr_ack(values, verify):
    addr   = verify["addr"].upper()
    target = f"ADDRW:{addr}"
    acked  = [v for v in values if target in v and "NACK" not in v]
    if acked:
        return True, f"Dia chi 0x{addr} ACK ({len(acked)} lan)"
    target_r = f"ADDRR:{addr}"
    acked_r  = [v for v in values if target_r in v and "NACK" not in v]
    if acked_r:
        return True, f"Dia chi 0x{addr} ACK read ({len(acked_r)} lan)"
    return False, f"Khong thay dia chi 0x{addr} duoc ACK"


def check_dht11_checksum(values, verify):
    min_count = verify.get("min_count", 3)
    valid     = 0
    invalid   = 0

    for v in values:
        parts = v.strip().split()
        if len(parts) != 5:
            continue
        try:
            b = [int(x, 16) for x in parts]
        except ValueError:
            continue
        calc = sum(b[:4]) & 0xFF
        if calc == b[4]:
            valid += 1
        else:
            invalid += 1

    if valid >= min_count:
        return True, f"DHT11 checksum OK: {valid} lan hop le, {invalid} lan loi"
    return False, f"DHT11 checksum: chi {valid} lan hop le (can {min_count}), {invalid} lan loi"


# ─── Dispatch ─────────────────────────────────────────────────────────────────

CHECK_FUNCTIONS = {
    "contains":          check_contains,
    "regex":             check_regex,
    "regex_value_range": check_regex_value_range,
    "i2c_addr_ack":      check_i2c_addr_ack,
    "dht11_checksum":    check_dht11_checksum,
}


def run_test_case(tc, rows):
    verify     = tc["verify"]
    source     = verify["source"]
    check_type = verify["type"]
    points     = tc["points"]

    values = get_column_values(rows, source)

    fn = CHECK_FUNCTIONS.get(check_type)
    if fn is None:
        return False, 0, f"[ERROR] Check type '{check_type}' chua duoc ho tro"

    try:
        passed, detail = fn(values, verify)
    except Exception as e:
        return False, 0, f"[ERROR] {e}"

    score = points if passed else 0
    return passed, score, detail


# ─── Grade ────────────────────────────────────────────────────────────────────

def grade(lab_name, json_path, csv_path=None):
    """
    Doc result.csv → cham diem theo test cases trong labX.json.
    csv_path=None → dung CSV_PATH mac dinh.
    """
    path = csv_path or CSV_PATH

    print(f"\n{'='*60}")
    print(f" CHECK LOGIC: {lab_name}")
    print(f" CSV   : {path}")
    print(f" JSON  : {json_path}")
    print(f"{'='*60}")

    if not os.path.exists(json_path):
        print(f" [ERROR] Khong tim thay JSON: {json_path}")
        return None

    if not os.path.exists(path):
        print(f" [ERROR] Khong tim thay CSV: {path}")
        return None

    rows = load_csv(path)

    with open(json_path, encoding="utf-8") as f:
        cfg = json.load(f)

    test_cases   = cfg.get("test_cases", [])
    total_points = sum(tc["points"] for tc in test_cases)
    earned       = 0
    results      = []

    for tc in test_cases:
        passed, score, detail = run_test_case(tc, rows)
        earned += score
        status = "PASS" if passed else "FAIL"
        results.append({
            "id":     tc["id"],
            "name":   tc["name"],
            "status": status,
            "score":  score,
            "max":    tc["points"],
            "detail": detail,
        })
        print(f"\n [{status}] {tc['id']}: {tc['name']}")
        print(f"        Diem: {score}/{tc['points']}")
        print(f"        {detail}")

    percent = round(earned / total_points * 100) if total_points else 0
    overall = "PASS" if percent >= 50 else "FAIL"

    print(f"\n{'='*60}")
    print(f" LOGIC DIEM: {earned}/{total_points} ({percent}%) — {overall}")
    print(f"{'='*60}\n")

    return {
        "lab":        lab_name,
        "timestamp":  datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        "score":      earned,
        "max":        total_points,
        "percent":    percent,
        "overall":    overall,
        "test_cases": results,
    }


# ─── Entry point ──────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="Autograder Check Logic")
    parser.add_argument("--lab",  required=True, help="Ten lab (vd: lab5-i2c)")
    parser.add_argument("--csv",  default=None,  help="Duong dan CSV (mac dinh: logic/result.csv)")
    parser.add_argument("--json", default=None,  help="Duong dan JSON")
    args = parser.parse_args()

    lab_name = args.lab

    lab_num   = re.search(r"lab(\d+)", lab_name)
    json_path = args.json or (
        os.path.join(ASSIGNMENTS_DIR, lab_name, f"lab{lab_num.group(1)}.json")
        if lab_num else None
    )
    if not json_path:
        print(f"[ERROR] Khong xac dinh duoc json path tu lab name: {lab_name}")
        return

    grade(lab_name, json_path, csv_path=args.csv)


if __name__ == "__main__":
    main()
