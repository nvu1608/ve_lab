#!/usr/bin/env python3
"""
check_reg.py — Kiểm tra cấu hình thanh ghi MCU qua pyOCD (SWD)

Dùng độc lập:
  python3 check_reg.py --lab lab5-i2c
  python3 check_reg.py --lab lab5-i2c --serial 37FF71064E573436DB011543

Import từ checker.py:
  import check_reg
  results = check_reg.run(register_checks, target=..., serial=...)
"""

import json
import re
import os
import argparse

# ─── Config ───────────────────────────────────────────────────────────────────

ASSIGNMENTS_DIR  = "/home/pi/ve_lab/assignment"
STLINK_STUDENT   = "37FF71064E573436DB011543"
DEFAULT_TARGET   = "stm32f103c8"

# ─── Core ─────────────────────────────────────────────────────────────────────

def run(register_checks, target=DEFAULT_TARGET, serial=STLINK_STUDENT):
    """
    Đọc và kiểm tra từng entry trong register_checks (từ JSON).

    Mỗi entry:
      { "id", "name", "points", "address", "mask", "expected", "comment" }

    Trả về list dict tương thích format test_cases của checker.py:
      { "id", "name", "status", "score", "max", "detail" }
    """
    try:
        from pyocd.core.helpers import ConnectHelper
    except ImportError:
        print(" [ERROR] check_reg: pyocd chưa được cài (pip install pyocd)")
        return _all_fail(register_checks, "pyocd not installed")

    if not register_checks:
        return []

    results = []

    print(f"\n{'='*60}")
    print(f" REGISTER CHECK (pyOCD)")
    print(f" Target : {target}")
    print(f" Serial : {serial}")
    print(f"{'='*60}")

    try:
        with ConnectHelper.session_with_chosen_probe(
            unique_id=serial,
            options={"target_override": target}
        ) as session:
            t = session.board.target
            t.halt()

            try:
                for rc in register_checks:
                    _check_one(t, rc, results)
            finally:
                t.resume()
                print(" [OK] resume CPU")  # đổi log → print

    except Exception as e:
        print(f" [ERROR] pyOCD connect/read: {e}")
        # Các entry chưa được check → thêm FAIL
        checked_ids = {r["id"] for r in results}
        for rc in register_checks:
            if rc["id"] not in checked_ids:
                results.append(_fail_entry(rc, f"pyOCD error: {e}"))

    # Tổng kết
    earned = sum(r["score"] for r in results)
    total  = sum(r["max"]   for r in results)
    print(f"\n REGISTER TOTAL: {earned}/{total}")
    print(f"{'='*60}\n")

    return results


def _check_one(target, rc, results):
    """Đọc 1 thanh ghi, append kết quả vào results."""
    try:
        addr     = int(rc["address"], 16)
        mask     = int(rc["mask"],    16)
        expected = int(rc["expected"],16)

        raw    = target.read32(addr)
        actual = raw & mask
        passed = (actual == expected)

        status = "PASS" if passed else "FAIL"
        detail = (
            f"addr={rc['address']}  "
            f"raw=0x{raw:08X}  "
            f"masked=0x{actual:08X}  "
            f"expected=0x{expected:08X}"
        )

        score = rc["points"] if passed else 0
        print(f"\n [{status}] {rc['id']}: {rc['name']}")
        print(f"        {detail}")
        if "comment" in rc:
            print(f"        ({rc['comment']})")

        results.append({
            "id":     rc["id"],
            "name":   rc["name"],
            "status": status,
            "score":  score,
            "max":    rc["points"],
            "detail": detail,
        })

    except Exception as e:
        err = f"read error: {e}"
        print(f"\n [ERROR] {rc['id']}: {err}")
        results.append(_fail_entry(rc, err))


def _fail_entry(rc, reason):
    return {
        "id":     rc["id"],
        "name":   rc["name"],
        "status": "FAIL",
        "score":  0,
        "max":    rc["points"],
        "detail": reason,
    }


def _all_fail(register_checks, reason):
    return [_fail_entry(rc, reason) for rc in register_checks]


# ─── Standalone ───────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="Register checker via pyOCD")
    parser.add_argument("--lab",    required=True, help="Tên lab (vd: lab5-i2c)")
    parser.add_argument("--serial", default=STLINK_STUDENT, help="STLink serial")
    parser.add_argument("--target", default=DEFAULT_TARGET,  help="pyOCD target")
    parser.add_argument("--json",   default=None, help="Đường dẫn JSON (tùy chọn)")
    args = parser.parse_args()

    # Resolve JSON path
    lab_num = re.search(r"lab(\d+)", args.lab)
    json_path = args.json or (
        os.path.join(ASSIGNMENTS_DIR, args.lab, f"lab{lab_num.group(1)}.json")
        if lab_num else None
    )
    if not json_path or not os.path.exists(json_path):
        print(f"[ERROR] Không tìm thấy JSON: {json_path}")
        return

    with open(json_path, encoding="utf-8") as f:
        cfg = json.load(f)

    register_checks = cfg.get("register_checks", [])
    if not register_checks:
        print("[WARN] JSON không có trường 'register_checks'")
        return

    target = cfg.get("target", args.target)
    run(register_checks, target=target, serial=args.serial)


if __name__ == "__main__":
    main()
