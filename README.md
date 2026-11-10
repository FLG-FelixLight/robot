# Robot dò line ESP32-S3

Xe robot tự hành dò line 5 mắt cảm biến trên nền tảng **ESP32-S3** (Arduino framework + PlatformIO),
có điều khiển web qua WiFi (WebSocket), chọn điểm đến, hiển thị LED 7 đoạn, phát âm thanh và cảm biến siêu âm chống vật cản.

## Chức năng chính

- Dò line 5 cảm biến (S1..S5) với bộ điều khiển PID.
- Nhận dạng ngã ba / ngã tư, rẽ trái - phải - đi thẳng theo bản đồ đường đi.
- Chọn điểm đến (destination) và hiển thị trạng thái trên web.
- Web server nhúng (LittleFS) + WebSocket telemetry ~100 ms/lần.
- Hiển thị đồng hồ / trạng thái bằng TM1637.
- Phát file MP3 (I2S + MAX98357) lưu trong `data/audio/`.
- Cảm biến siêu âm HC-SR04: dừng khi vật cản < 10 cm, chạy tiếp khi > 15 cm.

## Cấu trúc thư mục

```
platformio.ini        Cấu hình build & thư viện (lib_deps)
src/                  Mã nguồn firmware (main.cpp + các module)
data/                 Frontend nhúng LittleFS (index.html, app.js, style.css, tables.json, audio/)
include/ , lib/       Thư mục mặc định của PlatformIO
test/                 Thư mục test
*.md, *.txt           Tài liệu phân tích, báo cáo, slide
```

Phần cứng / chân GPIO được khai báo tập trung tại `src/robot_config.h`.

## Yêu cầu

- [PlatformIO](https://platformio.org/) (VS Code extension hoặc `pio` CLI)
- Board: `esp32-s3-devkitc-1`
- Kết nối phần cứng tương ứng với các chân trong `src/robot_config.h`:
  - Cảm biến line: S1=4, S2=5, S3=6, S4=7, S5=15
  - Driver L298N: ENA=16, IN1=17, IN2=18, IN3=8, IN4=3, ENB=46
  - I2S audio: LRC=9, BCLK=10, DIN=11, AMP_GAIN=12, AMP_SD=13
  - TM1637: CLK=1, DIO=2
  - HC-SR04: TRIG=45, ECHO=48, OUT=47

> Thư viện được PlatformIO tự tải theo `platformio.ini`, không cần commit `.pio/`:
> `ESP32Async/AsyncTCP`, `ESP32Async/ESPAsyncWebServer`, `bblanchon/ArduinoJson`, `earlephilhower/ESP8266Audio`.

## Build & nạp firmware

```bash
git clone https://github.com/FLG-FelixLight/robot.git
cd robot

pio run                 # build
pio run -t upload       # nạp firmware
pio run -t uploadfs     # nạp frontend trong data/ lên LittleFS
pio device monitor -b 115200
```

## Sử dụng

1. Nạp firmware và filesystem (`upload` + `uploadfs`).
2. Cấp nguồn, kết nối WiFi theo `WIFI_SSID` / `WIFI_PASS` trong `src/robot_config.h`.
3. Mở `http://robot.local` (mDNS) hoặc địa chỉ IP in ra Serial để vào giao diện điều khiển.
4. Chọn điểm đến trên web, robot tự dò line tới đích.

## Lưu ý

- `src/robot_config.h` đang chứa sẵn SSID/mật khẩu WiFi mẫu — hãy đổi thành mạng của bạn trước khi nạp.
- Thư mục `.pio/` (build + thư viện tải về) được `.gitignore` bỏ qua để repo gọn nhẹ.

## Tài liệu

- `FLOW_TOAN_BO_DU_AN.md` — luồng hoạt động toàn bộ dự án
- `BAO_CAO_SO_BO.md` — báo cáo sơ bộ
- `DRAFT_PAPER_OUTLINE.md` — dàn ý bài báo
- `slide-*.txt` — nội dung slide (ý tưởng, map đường đi, hạn chế & hướng phát triển)
