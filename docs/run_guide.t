HƯỚNG DẪN CHẠY AUTOGRADER TRÊN RASPBERRY PI
==========================================

1. Cấu trúc lệnh cơ bản
-----------------------
Để chạy toàn bộ quy trình (Build -> Flash -> Reg Check -> Capture -> Grade):
$ python3 /home/pi/ve_lab/logic/main.py --lab <tên_thư_mục_lab>

Ví dụ:

python3 /home/pi/ve_lab/logic/main.py --lab lab6_ds1307


2. Chạy từng module riêng lẻ (Manual Mode)
------------------------------------------
Bạn có thể sử dụng các flag sau để chạy riêng từng phần:

- Chỉ Build code:
   python3 /home/pi/ve_lab/logic/main.py --lab lab6_ds1307 --build

- Chỉ Flash firmware:
   python3 /home/pi/ve_lab/logic/main.py --lab lab6_ds1307 --flash

- Chỉ Kiểm tra thanh ghi:
   python3 /home/pi/ve_lab/logic/main.py --lab lab6_ds1307 --reg

- Chỉ Capture tín hiệu (đo logic):
   python3 /home/pi/ve_lab/logic/main.py --lab lab6_ds1307 --capture --time 20

- Chỉ Chấm điểm (dựa trên result.csv có sẵn):
   python3 /home/pi/ve_lab/logic/main.py --lab lab6_ds1307 --grade


3. Lưu ý về đường dẫn Source Code (GITHUB_WORKSPACE)
----------------------------------------------------
Khi chạy thủ công trên Pi (ngoài GitHub Actions), nếu code sinh viên nằm trong thư mục của action-runner, bạn cần set biến môi trường GITHUB_WORKSPACE trỏ về thư mục CHA của thư mục lab.

Cú pháp:
$ GITHUB_WORKSPACE=<đường_dẫn_thư_mục_cha> python3 /home/pi/ve_lab/logic/main.py --lab <tên_lab> --build

Ví dụ thực tế:
$ GITHUB_WORKSPACE=/home/pi/actions-runner/_work/my_autograder/my_autograder python3 /home/pi/ve_lab/logic/main.py --lab lab6_ds1307 --build


4. Các file kết quả
-------------------
- File đo tín hiệu: /home/pi/ve_lab/logic/result.csv
- File log của Sigrok: Sẽ hiển thị trực tiếp trên terminal với prefix [SIGROK].
