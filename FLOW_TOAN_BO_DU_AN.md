# FLOW TOÀN BỘ DỰ ÁN XE TỰ HÀNH DÒ LINE ESP32-S3

## 1. Bối cảnh, mục tiêu và phạm vi của dự án

Dự án được xây dựng dựa trên bài toán giao đồ trong nhà hàng, khách sạn hoặc trong một không gian có sẵn sơ đồ đường đi. Công việc giao đồ thủ công lặp đi lặp lại, người phục vụ phải đi nhiều lần và khách phải chờ đợi. Ý tưởng của dự án là tạo ra một robot tự hành cỡ nhỏ có khả năng nhận một điểm đến, tự đi theo vạch dẫn đường và dừng đúng vị trí đã chọn. Robot không dùng camera, không dùng LiDAR và không tự lập bản đồ. Toàn bộ không gian hoạt động được đơn giản hóa thành một sa bàn có vạch kẻ sẵn, các ngã ba, ngã tư và các điểm dừng. Trong phạm vi đó, robot chỉ cần giải quyết bốn việc: bám được vạch, nhận diện được ngã rẽ, đi đúng thứ tự các ngã rẽ theo một mã đường đi và dừng khi gặp điểm kết thúc.

Mục tiêu thứ nhất là xây dựng chế độ bám line ổn định bằng bộ điều khiển PID trên nền vi điều khiển ESP32-S3. Năm mắt hồng ngoại được dùng để xác định vị trí tương đối của vạch so với thân xe. Hai động cơ DC được điều khiển qua mô-đun L298N. Sai lệch vị trí được đưa vào PID để tính ra mức bù tốc độ cho bánh trái và bánh phải, nhờ đó xe bám vạch thay vì chỉ chạy theo một tốc độ cố định.

Mục tiêu thứ hai là điều khiển và giám sát robot theo thời gian thực qua web. ESP32-S3 kết nối vào mạng WiFi 2.4 GHz, tự dựng một web server và một WebSocket. Người dùng mở trình duyệt trên điện thoại hoặc laptop trong cùng mạng, xem trạng thái của năm cảm biến, sai lệch, tốc độ hai bánh, chế độ hoạt động, địa chỉ IP và cường độ tín hiệu WiFi. Người dùng cũng có thể chỉnh Kp, Ki, Kd và tốc độ nền ngay trên web mà không cần nạp lại firmware.

Mục tiêu thứ ba là chế độ chọn điểm đến. Mỗi điểm đến không được lưu bằng tọa độ mà được mã hóa bằng một chuỗi ký tự. Ký tự L nghĩa là rẽ trái ở ngã rẽ kế tiếp, R nghĩa là rẽ phải, S nghĩa là đi thẳng qua ngã rẽ, và 0 nghĩa là đi tới ngã rẽ kế tiếp rồi dừng. Đây là một route code tuần tự dành cho sa bàn đã biết trước; chỉ có thể xem nó như cây nhị phân nếu sơ đồ thực sự là cây và điểm xuất phát/nút rẽ được định nghĩa rõ. Firmware không biết tọa độ hay khoảng cách giữa các nút. Muốn đổi điểm đến, người dùng đổi chuỗi trong bảng điểm đến mà không cần sửa thuật toán hay biên dịch lại chương trình.

Mục tiêu thứ tư là phát âm thanh ngay trên robot. Một mô-đun MAX98357 nhận dữ liệu I2S từ ESP32-S3 và khuếch đại ra loa. File MP3 được lưu trong bộ nhớ flash LittleFS. Việc giải mã MP3 được đặt trong một task riêng để không làm chậm vòng điều khiển động cơ.

Mục tiêu bổ sung là hiển thị giờ Việt Nam dạng HH:MM trên TM1637. ESP32-S3 đồng bộ đồng hồ hệ thống với NTP qua Wi-Fi, sau đó cập nhật phần hiển thị mỗi giây. HY-SRF05 đo khoảng cách phía trước; khi khoảng cách nhỏ hơn 10 cm, firmware dừng động cơ nhưng vẫn giữ chế độ và trạng thái đường đi. Khi khoảng cách đạt ít nhất 15 cm hoặc liên tiếp không nhận được echo, firmware tiếp tục chế độ trước đó. Khoảng 10–15 cm tạo vùng trễ để tránh đóng/ngắt động cơ liên tục.

Phạm vi của phiên bản hiện tại là một robot chạy trên sa bàn có vạch kẻ sẵn. Dự án chưa có encoder để đo tốc độ thực, chưa có camera, chưa tự lập bản đồ và chưa có cơ chế tự quay về điểm xuất phát. Remote hồng ngoại có trong ý tưởng ban đầu nhưng chưa được tích hợp vào firmware hiện tại. Những giới hạn này được nêu rõ để người đọc phân biệt giữa phần đã hoàn thành và phần nằm trong hướng phát triển.

## 2. Kiến trúc tổng thể của hệ thống

Hệ thống gồm bốn khối chính: khối thu thập thông tin, khối xử lý trung tâm, khối chấp hành và khối giao tiếp người dùng. Khối thu thập thông tin là mô-đun năm mắt dò line. Firmware hiện quy ước mức HIGH là thấy line (`LINE_ON = HIGH`); cực tính thật cần được kiểm tra với đúng module. Theo sơ đồ chân người dùng cung cấp, năm ngõ ra nối GPIO 4, 5, 6, 7 và 15. Điện áp ngõ ra phải nằm trong giới hạn chịu đựng của ESP32-S3; cần xác minh mức điện áp thực tế và không mặc định rằng board cảm biến được cấp 3.3 V nếu chưa kiểm tra.

Khối xử lý trung tâm là board ESP32-S3 DevKitC-1. Board này đảm nhiệm toàn bộ logic: đọc cảm biến, tính sai lệch, chạy PID, nhận diện ngã rẽ, quản lý bảng điểm đến, dựng web server, chạy WebSocket, phát telemetry và điều phối task âm thanh. Việc chọn ESP32-S3 xuất phát từ nhu cầu vừa điều khiển động cơ theo thời gian thực vừa chạy web server vừa giải mã MP3. Chip có hai nhân, có WiFi tích hợp, có ngoại vi I2S và có đủ GPIO cho dự án.

Khối chấp hành gồm mô-đun L298N, hai động cơ DC giảm tốc và mô-đun âm thanh MAX98357. L298N nhận tín hiệu PWM và tín hiệu chiều quay từ ESP32-S3 để điều khiển hai động cơ. Các chân điều khiển gồm ENA ở GPIO 16, IN1 ở GPIO 17, IN2 ở GPIO 18 cho bánh trái; IN3 ở GPIO 8, IN4 ở GPIO 3 và ENB ở GPIO 46 cho bánh phải. MAX98357 nhận tín hiệu I2S gồm LRC ở GPIO 9, BCLK ở GPIO 10 và DIN ở GPIO 11; GAIN ở GPIO 12 được giữ mức LOW để chọn mức khuếch đại thấp, SD ở GPIO 13 được kéo mức HIGH để bật bộ khuếch đại. TM1637 dùng CLK GPIO 1 và DIO GPIO 2. HY-SRF05 dùng TRIG GPIO 45, ECHO GPIO 48; GPIO 47 nối OUT nhưng firmware để INPUT và chỉ đo khoảng cách qua TRIG/ECHO.

Khối giao tiếp người dùng là trình duyệt trên điện thoại hoặc laptop. Trình duyệt tải giao diện từ LittleFS của ESP32-S3, sau đó mở một kết nối WebSocket đến đường dẫn /ws. Mọi lệnh điều khiển đi từ trình duyệt xuống ESP32-S3 qua WebSocket. Mọi trạng thái đi từ ESP32-S3 lên trình duyệt cũng qua WebSocket. Bảng điểm đến được quản lý qua REST API tại đường dẫn /api/tables.

Về nguồn, hai pin 18650 mắc nối tiếp cung cấp khoảng 7.4 V cho L298N để nuôi động cơ. Một mạch buck hạ áp xuống 5 V để cấp cho ESP32-S3 và MAX98357. Cảm biến dùng 3.3 V từ board. Tất cả các khối phải nối chung GND. Động cơ không được nuôi từ chân 5 V của ESP32-S3 vì dòng khởi động của động cơ lớn hơn nhiều so với khả năng cấp của board.

Luồng năng lượng và luồng thông tin tách biệt nhau. Năng lượng đi từ pin đến L298N và mạch buck, rồi đến động cơ, board và mô-đun âm thanh. Thông tin đi từ cảm biến vào ESP32-S3, được xử lý thành lệnh PWM, rồi ra L298N và động cơ. Thông tin từ người dùng đi qua WiFi vào ESP32-S3 và hòa vào cùng vòng xử lý đó.

## 3. Cấu trúc dự án và hai vùng bộ nhớ

Dự án được tổ chức theo chuẩn PlatformIO. Tệp platformio.ini khai báo môi trường esp32-s3-devkitc-1, framework Arduino, hệ thống tệp LittleFS và các thư viện phụ thuộc. Các thư viện chính gồm AsyncTCP, ESPAsyncWebServer, ArduinoJson phiên bản 7 và ESP8266Audio phiên bản 2.0.0. Tệp cấu hình cũng đặt tốc độ Serial Monitor là 115200.

Thư mục src chia firmware theo trách nhiệm. Tệp main.cpp giữ setup() và loop() để điều phối; robot_config.h chứa cấu hình chân và hằng số; robot_state.h/.cpp khai báo và định nghĩa trạng thái dùng chung. Các module motor_control, destination_manager, path_control, web_control, audio_control, network, clock_display và obstacle_monitor lần lượt phụ trách động cơ/cảm biến/PID, bảng điểm đến, điều hướng ngã rẽ, giao tiếp web, phát âm thanh, Wi-Fi, đồng hồ TM1637/NTP và đo khoảng cách/chặn vật cản. Cách tách này giúp tìm và bảo trì từng phần mà vẫn giữ vòng điều khiển chính tập trung trong main.cpp.

Thư mục data chứa các tài nguyên được nạp vào LittleFS. Đó là index.html, app.js, style.css, tables.json và thư mục audio chứa welcome.mp3. Đây là giao diện web hoàn chỉnh, không phụ thuộc vào Internet. Khi trình duyệt yêu cầu trang chủ, ESP32-S3 trả về index.html từ LittleFS; trình duyệt tiếp tục yêu cầu style.css và app.js cũng từ LittleFS.

Thư mục include, lib và test là các thư mục khung do PlatformIO tạo ra, hiện chỉ chứa tệp README và không tham gia vào chức năng chính. Thư mục .vscode chứa cấu hình soạn thảo. Thư mục .pio chứa thư viện đã tải, mã đối tượng và sản phẩm biên dịch; đây là thư mục sinh ra tự động, không phải mã nguồn của dự án.

Điểm quan trọng là firmware và giao diện web nằm ở hai vùng bộ nhớ khác nhau. Firmware được nạp bằng lệnh upload, còn giao diện, bảng điểm đến và file âm thanh được nạp bằng lệnh uploadfs. Khi thay đổi giao diện hoặc thêm file MP3, chỉ cần nạp lại filesystem. Khi thay đổi logic điều khiển, chỉ cần nạp lại firmware. Lệnh nạp filesystem sẽ xóa và ghi lại toàn bộ vùng LittleFS, vì vậy cần lưu ý nếu đã chỉnh bảng điểm đến trực tiếp trên robot.

Ngoài mã nguồn, thư mục dự án còn có báo cáo sơ bộ, các tệp ghi chú nội dung slide, một tệp trình chiếu và các tài liệu mô tả ý tưởng, mã đường đi, hạn chế và hướng phát triển. Những tài liệu này không ảnh hưởng đến hoạt động của robot nhưng phản ánh quá trình phân tích và thiết kế của nhóm.

## 4. Luồng khởi động của firmware

Khi cấp nguồn, chương trình bắt đầu ở hàm setup. Việc đầu tiên là mở cổng Serial ở tốc độ 115200 và chờ 300 ms để cổng ổn định. Sau đó firmware cấu hình năm chân cảm biến là INPUT. Các chân của L298N gồm ENA, IN1, IN2, IN3, IN4 và ENB được cấu hình là OUTPUT. Ngay sau khi cấu hình, firmware gọi hàm dừng động cơ để đảm bảo robot không chạy ngoài ý muốn trong lúc khởi động.

Tiếp theo, chân GAIN của MAX98357 được kéo LOW trước khi chân SD được kéo HIGH để bật bộ khuếch đại. Firmware khởi tạo TM1637 và HY-SRF05; TRIG là output, ECHO là input, chân OUT để input. Sau đó firmware khởi tạo LittleFS. Nếu việc mount thất bại, firmware yêu cầu định dạng lại filesystem. Cơ chế này giúp board vẫn khởi động được khi vùng flash bị lỗi hoặc mới được nạp lần đầu, nhưng đồng thời cũng có nghĩa là dữ liệu cũ có thể bị xóa nếu filesystem hỏng.

Sau khi LittleFS sẵn sàng, firmware gọi hàm loadTables để đọc tệp /tables.json. Hàm này kiểm tra tệp có tồn tại hay không, mở tệp, phân tích JSON, rồi duyệt từng cặp khóa và giá trị. Khóa được chuyển thành số nguyên và phải lớn hơn 0. Giá trị phải là chuỗi và phải vượt qua hàm kiểm tra pathValid. Những mục không hợp lệ bị bỏ qua và ghi log ra Serial. Những mục hợp lệ được đưa vào một std::map tạm, sau đó gán vào biến toàn cục gTables. Nếu không có mục nào hợp lệ, bảng rỗng và chế độ điểm đến sẽ không thể tìm thấy mã.

Tiếp theo, firmware tạo đối tượng AudioOutputI2S, cấu hình chân BCLK, LRC, DIN và đặt mức khuếch đại 0.30. Sau đó firmware tạo task audio bằng xTaskCreatePinnedToCore với stack 8192 byte, độ ưu tiên 1, ghim vào core 0. Task này chạy vô hạn và chịu trách nhiệm giải mã MP3, tách hoàn toàn khỏi vòng điều khiển.

Firmware tạo một hàng đợi FreeRTOS có 8 phần tử, mỗi phần tử là một cấu trúc Command. Hàng đợi này là cầu nối giữa task WebSocket và vòng loop. Việc tạo hàng đợi trước khi kết nối WiFi đảm bảo rằng khi lệnh đầu tiên từ web đến, nơi nhận đã sẵn sàng.

Sau đó firmware gọi connectWiFi. Hàm này đặt chế độ WiFi là STA, đặt hostname là robot-line, ngắt kết nối cũ và chờ 200 ms. Firmware quét các mạng xung quanh và in ra Serial tên mạng, cường độ tín hiệu và kênh. Nếu không tìm thấy đúng tên mạng đã cấu hình, firmware in cảnh báo và kết thúc quá trình kết nối. Nếu tìm thấy, firmware gọi WiFi.begin và chờ tối đa 20 giây. Khi kết nối thành công, firmware in địa chỉ IP và cường độ tín hiệu. Khi thất bại, firmware in lý do thất bại và để vòng loop thử kết nối lại sau mỗi 5 giây.

Sau lần thử kết nối Wi-Fi ban đầu, firmware khởi chạy đồng bộ NTP với múi giờ ICT-7 (UTC+7). Việc hiển thị giờ vẫn chạy theo đồng hồ hệ thống mỗi giây; khi chưa có thời gian hợp lệ, TM1637 hiển thị dấu gạch.

Sau WiFi, firmware khởi tạo mDNS với tên robot. Nếu thành công, robot có thể được truy cập bằng địa chỉ http://robot.local/ thay vì phải nhớ IP. Đây là chi tiết thuận tiện cho việc demo, nhưng vẫn cần cùng mạng và router hỗ trợ mDNS.

Tiếp theo, firmware gắn hàm xử lý sự kiện cho WebSocket tại đường dẫn /ws và thêm handler này vào web server. Sau đó firmware đăng ký REST API. Route GET /api/tables trả về toàn bộ bảng điểm đến dưới dạng JSON. Route PUT /api/tables nhận dữ liệu theo từng khối, ghép lại thành chuỗi, kiểm tra và ghi vào LittleFS. Hai route này được đăng ký trước route phục vụ tệp tĩnh để tránh bị route tĩnh nuốt mất.

Sau các API, firmware gọi serveStatic cho đường dẫn gốc, trỏ vào LittleFS và đặt tệp mặc định là index.html. Mọi yêu cầu không khớp sẽ nhận phản hồi 404. Cuối cùng web server bắt đầu lắng nghe ở cổng 80 và firmware in địa chỉ truy cập ra Serial.

Trạng thái sau khi setup hoàn tất là robot đang dừng, robotEnabled bằng false và robotMode bằng MODE_STOP. Việc khởi động không tự động chạy động cơ. Người dùng phải chủ động gửi lệnh START, lệnh lái tay hoặc lệnh GOTO thì robot mới chuyển sang chế độ hoạt động. Đây là lựa chọn an toàn cho việc thử nghiệm và demo.

## 5. Luồng mở web, kết nối WebSocket và đồng bộ ban đầu

Khi người dùng mở trình duyệt và truy cập địa chỉ IP của robot hoặc robot.local, trình duyệt gửi yêu cầu GET đến đường dẫn gốc. ESP32-S3 trả về index.html từ LittleFS. Trang HTML tham chiếu style.css và app.js, nên trình duyệt tiếp tục tải hai tệp này. Toàn bộ giao diện nằm trong bộ nhớ flash của robot, không cần kết nối Internet.

Khi app.js chạy, nó khởi tạo các tham chiếu đến phần tử giao diện, thiết lập tab, vẽ đồ thị rỗng, hiển thị bảng điểm đến trống, gọi loadTables để tải bảng từ REST API và gọi connect để mở WebSocket. Địa chỉ WebSocket được tạo từ giao thức và host hiện tại, nghĩa là nếu trang được mở bằng http thì dùng ws, nếu mở bằng https thì dùng wss.

Khi WebSocket mở thành công, giao diện chuyển chấm trạng thái sang màu xanh và ghi log đã kết nối. Ở phía firmware, sự kiện WS_EVT_CONNECT được kích hoạt. Firmware gửi ngay một bản tin config cho client vừa kết nối và gửi một bản tin telemetry. Nhờ vậy giao diện có ngay các giá trị Kp, Ki, Kd, tốc độ nền, tốc độ tối đa, trạng thái bật tắt, chế độ, điểm đến hiện tại và hướng lái tay. Giao diện cũng có ngay trạng thái của năm cảm biến, sai lệch, tốc độ hai bánh, thông báo, IP và RSSI mà không phải chờ chu kỳ telemetry kế tiếp.

Nếu mất kết nối, app.js chuyển chấm trạng thái sang màu đỏ, ghi log và đặt hẹn kết nối lại sau 1.5 giây. Cơ chế này giúp giao diện tự phục hồi khi WiFi chập chờn hoặc khi người dùng chuyển tab trên điện thoại. Ở phía firmware, nếu WiFi mất, vòng loop kiểm tra mỗi 5 giây và gọi WiFi.reconnect. Trong thời gian mất mạng, vòng điều khiển vẫn chạy độc lập.

Khi tải bảng điểm đến, app.js gọi fetch đến /api/tables. Firmware trả về một đối tượng JSON với khóa là ID và giá trị là mã đường đi. app.js chuyển các khóa thành số, sắp xếp tăng dần và hiển thị thành danh sách. Mỗi dòng có ID, mã, nút Đi và các nút Sửa, Xóa. Nếu bảng rỗng, giao diện hiển thị dòng thông báo chưa có dữ liệu.

## 6. Luồng gửi lệnh từ web xuống robot

Toàn bộ lệnh điều khiển được gửi dưới dạng JSON qua WebSocket. app.js có hàm send kiểm tra kết nối đang mở rồi gọi ws.send. Các lệnh hiện có gồm start, stop, estop, drive, goto, play và config; firmware cũng còn hỗ trợ `tables_reload` như lệnh đọc lại bảng. PUT thành công cập nhật map trong RAM nên giao diện không cần gửi `tables_reload` sau khi lưu.

Ở phía firmware, khi có dữ liệu WebSocket, hàm onWsEvent kiểm tra frame có phải là frame văn bản hoàn chỉnh hay không. Nếu đúng, firmware kết thúc chuỗi bằng ký tự null, tạo một String và gọi handleCommand. Hàm này phân tích JSON, đọc trường cmd và chuyển thành một cấu trúc Command.

Lệnh start được chuyển thành CMD_START. Lệnh stop thành CMD_STOP. Lệnh estop thành CMD_ESTOP. Lệnh drive đọc thêm trường dir và mặc định là stop. Lệnh goto đọc id, tra trong gTables. Nếu không tìm thấy ID, firmware đặt thông báo không có mã điểm đến, gửi config về web và dừng xử lý. Nếu tìm thấy, firmware chép mã đường đi vào trường text của Command và đặt value bằng ID. Lệnh play đọc tên file và mặc định là welcome.mp3. Lệnh config đọc kp, ki, kd, base; nếu thiếu trường nào thì giữ giá trị hiện tại. Lệnh tables_reload được xử lý ngay trong task WebSocket: gọi loadTables, đặt thông báo đã tải lại và gửi config, không đưa vào hàng đợi.

Với các lệnh còn lại, nếu Command hợp lệ và hàng đợi tồn tại, firmware gọi xQueueSend với thời gian chờ bằng 0. Nghĩa là nếu hàng đợi đầy, lệnh bị bỏ qua thay vì chặn task WebSocket. Hàng đợi có 8 chỗ, đủ cho các thao tác thông thường. Việc dùng hàng đợi là điểm thiết kế quan trọng: task WebSocket chạy trong thư viện bất đồng bộ, còn điều khiển động cơ chạy trong loop. Hai bên không gọi trực tiếp vào nhau, tránh tranh chấp dữ liệu và tránh việc một gói tin mạng làm sai nhịp PID.

Trong mỗi vòng loop, firmware rút toàn bộ lệnh đang có trong hàng đợi và gọi applyCommand cho từng lệnh. Hàm này thay đổi trạng thái robot một cách tuần tự. Sau khi xử lý xong, firmware gọi sendConfig để gửi lại cấu hình mới nhất cho tất cả client.

Lệnh CMD_START đặt lại trạng thái PID, bật robotEnabled, chuyển robotMode sang MODE_AUTO, xóa hướng lái tay, xóa đường đi, đặt chỉ số đường đi về 0, xóa điểm đến, xóa trạng thái rẽ và trạng thái nhận diện ngã rẽ. Thông báo là AUTO start.

Lệnh CMD_GOTO cũng đặt lại PID và bật robotEnabled, nhưng chuyển robotMode sang MODE_PATH. Firmware chép mã đường đi từ Command vào gPath, đặt gPathIdx bằng 0, gDestId bằng ID, xóa trạng thái rẽ và các biến nhận diện ngã rẽ. Thông báo hiển thị điểm đến và mã đường đi.

Lệnh CMD_STOP và CMD_ESTOP đều tắt robotEnabled, chuyển mode về STOP, xóa hướng lái tay, xóa trạng thái rẽ, xóa đường đi và chỉ số, gọi stopMotors, đặt tốc độ hai bánh về 0. Thông báo tương ứng là STOP hoặc E-STOP. Trong firmware hiện tại, hai lệnh này có cùng tác dụng dừng; E-STOP chỉ khác ở thông báo.

Lệnh CMD_DRIVE chép hướng lái vào driveDir. Nếu hướng là stop, firmware tắt robotEnabled, chuyển mode STOP, dừng động cơ và đặt tốc độ về 0. Nếu là hướng khác, firmware bật robotEnabled, chuyển mode MANUAL và cập nhật mốc thời gian nhận lệnh lái tay.

Lệnh CMD_CONFIG cập nhật Kp, Ki, Kd và baseSpeed. Các giá trị này có hiệu lực ngay ở vòng PID kế tiếp. Lệnh CMD_PLAY chép tên file vào gAudioFile và bật cờ gAudioNew để task audio xử lý.

Sau khi xử lý bất kỳ lệnh nào, sendConfig gửi lại cấu hình gồm Kp, Ki, Kd, baseSpeed, maxSpeed, trạng thái bật tắt, tên chế độ, điểm đến và hướng lái. Giao diện dùng bản tin này để đồng bộ slider và nhãn trạng thái.

## 7. Luồng đọc cảm biến, tính sai lệch và chạy PID

Trong mỗi vòng loop, firmware cập nhật phép đo HY-SRF05 và màn hình giờ. Nếu khoảng cách nhỏ hơn 10 cm, motor được dừng trước khi xử lý bất kỳ nhánh điều khiển chạy nào; WebSocket và các lệnh STOP/E-STOP vẫn được xử lý. Khi vật cản rời xa đến ít nhất 15 cm, firmware đặt lại bộ PID và bộ đếm ngã rẽ, rồi tiếp tục chế độ đang hoạt động. Màn hình TM1637 hiển thị giờ hệ thống theo HH:MM và nhấp nháy dấu hai chấm mỗi giây; trước khi NTP đồng bộ, hiển thị dấu gạch.

Sau đó firmware đọc năm cảm biến bằng digitalRead. Mỗi giá trị được so với hằng số LINE_ON, hiện được đặt là HIGH. Kết quả được lưu vào mảng gSensor dưới dạng 1 hoặc 0 để gửi lên web. Firmware đếm số mắt đang thấy line và đặt cờ gFullLine nếu cả năm mắt cùng thấy line.

Hàm readLine nhận năm giá trị cảm biến và gán trọng số đối xứng: S1 là -2, S2 là -1, S3 là 0, S4 là +1, S5 là +2. Hàm chỉ cộng trọng số của những mắt đang thấy line, sau đó chia cho số mắt đang thấy line. Kết quả là sai lệch vị trí tương đối. Nếu line nằm giữa, sai lệch bằng 0. Nếu line lệch sang trái, sai lệch âm. Nếu line lệch sang phải, sai lệch dương. Khi không có mắt nào thấy line, hàm trả về lastError, tức là sai lệch cuối cùng. Cách xử lý này giúp xe không bị giật ngay khi mất vạch trong thời gian ngắn, nhưng cũng là lý do xe có thể tiếp tục quay theo xu hướng cũ nếu mất vạch lâu.

Sai lệch được đưa vào calculatePID. Hàm lấy thời gian hiện tại bằng millis, tính dt theo giây so với lần gọi trước. Nếu dt nhỏ hơn hoặc bằng 0, hàm gán dt bằng 0.001 để tránh chia cho 0. Thành phần P là sai lệch hiện tại. Thành phần I cộng dồn sai lệch nhân dt và bị giới hạn trong khoảng -10 đến 10 để chống tích lũy quá mức. Thành phần D là hiệu giữa sai lệch hiện tại và sai lệch trước, chia cho dt. Sau đó hàm lưu sai lệch hiện tại vào lastError và trả về Kp nhân P cộng Ki nhân I cộng Kd nhân D.

Giá trị mặc định của bộ điều khiển là Kp bằng 35.0, Ki bằng 0.0 và Kd bằng 20.0. Tốc độ nền là 150 trên thang 255. Với bám line, thành phần P tạo phản ứng theo độ lệch, thành phần D giảm dao động, còn thành phần I mặc định tắt vì sai lệch vị trí không tích lũy theo kiểu điều khiển tốc độ. Người dùng vẫn có thể bật Ki trên web để thử nghiệm.

Từ giá trị correction, firmware tính tốc độ bánh trái bằng `baseSpeed - correction` và tốc độ bánh phải bằng `baseSpeed + correction`. Hai giá trị được giới hạn trong khoảng -maxSpeed đến maxSpeed. Sau đó driveMotors gọi motorLeft và motorRight, đồng thời lưu tốc độ vào gLeftSpeed và gRightSpeed để gửi telemetry.

Hàm motorLeft nhận tốc độ từ -255 đến 255. Nếu tốc độ dương, firmware đặt IN1 mức cao, IN2 mức thấp và xuất PWM bằng tốc độ lên ENA. Nếu tốc độ âm, firmware đảo IN1 và IN2 rồi xuất PWM bằng giá trị tuyệt đối. Nếu tốc độ bằng 0, firmware đặt cả hai chân điều hướng về mức thấp và PWM bằng 0. Hàm motorRight làm tương tự với IN3, IN4 và ENB.

Ở các nhánh điều khiển chính, vòng loop gọi `delay(LINE_LOOP_DELAY_MS)` với giá trị hiện tại là 5 ms. Đây chỉ là thời gian nghỉ; chu kỳ đọc thực còn gồm thời gian xử lý, Serial logging và các tác vụ hệ thống, nên chưa thể khẳng định tần số lấy mẫu 200 Hz nếu chưa đo. Telemetry không gửi ở mỗi vòng mà được giới hạn bởi hàm telemetryThrottled, khoảng 100 ms/lần (tối đa danh nghĩa 10 Hz).

## 8. Luồng chế độ STOP và chế độ lái tay

Ở chế độ STOP, mỗi vòng loop firmware gọi stopMotors, đặt tốc độ hai bánh về 0, in trạng thái cảm biến ra Serial, gửi telemetry nếu đến hạn và chờ 5 ms. Chế độ này là trạng thái mặc định sau khi khởi động và sau khi người dùng bấm STOP hoặc E-STOP. Robot vẫn đọc cảm biến và gửi telemetry.

Ở chế độ MANUAL, firmware kiểm tra thời gian từ lệnh lái tay gần nhất. Nếu đã quá 600 ms mà không có lệnh mới, firmware dừng động cơ và ghi log timeout. Nếu vẫn còn trong thời hạn, firmware gọi applyManualDrive để tính tốc độ theo hướng hiện tại.

Hàm applyManualDrive lấy tốc độ nhanh bằng baseSpeed giới hạn trong khoảng 0 đến maxSpeed, tốc độ chậm bằng một nửa tốc độ nhanh. Với hướng forward, cả hai bánh chạy tốc độ nhanh. Với back, cả hai bánh chạy lùi tốc độ nhanh. Với left, bánh trái dừng và bánh phải tiến, tạo chuyển động quay trái quanh bánh trái. Với right, bánh trái tiến và bánh phải dừng. Với forward_left, bánh trái chạy chậm và bánh phải chạy nhanh. Với forward_right, bánh trái chạy nhanh và bánh phải chạy chậm. Với back_left, bánh trái lùi chậm và bánh phải lùi nhanh. Với back_right, bánh trái lùi nhanh và bánh phải lùi chậm.

Ở phía giao diện, người dùng có thể giữ nút trên D-pad hoặc giữ phím mũi tên. app.js lưu các hướng đang được giữ trong một tập hợp. Khi hướng tổng hợp thay đổi, app.js gửi lệnh drive một lần. Nếu hướng khác stop, app.js đặt một bộ hẹn giờ gửi lặp mỗi 250 ms. Cơ chế heartbeat này đảm bảo firmware luôn nhận được lệnh mới trước khi hết 600 ms. Khi người dùng thả nút hoặc thả phím, app.js gửi lệnh stop và hủy bộ hẹn giờ. Nếu tab bị mất focus hoặc kết nối mạng bị đứt, firmware sẽ tự dừng sau 600 ms nhờ timeout.

Chế độ MANUAL không dùng PID và không đọc line để điều khiển. Cảm biến vẫn được đọc để hiển thị nhưng không ảnh hưởng đến tốc độ. Đây là chế độ dùng để di chuyển robot vào vị trí xuất phát hoặc kiểm tra chiều quay của động cơ.

## 9. Luồng chế độ PATH và nhận diện ngã rẽ

Chế độ PATH bắt đầu khi người dùng bấm nút Đi ở một dòng trong bảng điểm đến. Giao diện gửi lệnh goto kèm ID. Firmware tra bảng, lấy mã đường đi, đưa lệnh vào hàng đợi. Khi loop xử lý lệnh, robot chuyển sang MODE_PATH, bật robotEnabled, đặt gPath bằng mã, gPathIdx bằng 0 và gDestId bằng ID. Mỗi vòng lặp đọc các cảm biến; detector ngã rẽ chạy trước nhánh PID nhưng chỉ trong MODE_PATH, không có vật cản và không đang thực hiện cú rẽ. Nếu không xử lý ngã rẽ trong vòng đó, bộ điều khiển mới tiếp tục PID bám line.

Firmware nhận diện mẫu ngã rẽ khi trong dãy S1→S5 có `1`, tiếp theo ít nhất một `0`, rồi có `1` phía sau: pattern `1...0...1`. Các ví dụ `10001`, `11001`, `11101`, `11011` được nhận; `11100` không được nhận. Đây là kiểm tra chuỗi bit, không phải điều kiện `active >= 3` hay bắt buộc S3 = 1. Cấu hình hiện coi HIGH là line; nếu cảm biến thực tế active-low, cần sửa cấu hình hoặc cách đọc sau khi đo.

Khi pattern xuất hiện lần đầu, firmware ghi mốc `gJunctionSince`; nếu pattern mất trước 30 ms thì xóa mốc, nếu giữ liên tục đủ `JUNCTION_STABLE_MS = 30` ms thì mẫu qua debounce. Ngoài ra detector chỉ tiêu thụ mẫu nếu đã `gJunctionArmed`: trước đó phải có trạng thái không phải pattern, S3 thấy line và không quá 2 sensor active liên tục 200 ms (`CENTER_ARM_MS`). Vì vậy robot bắt đầu ngay tại junction hoặc không có đoạn bám giữa ổn định có thể chưa arm. Các ngưỡng này là cấu hình trong source, không phải kết quả hiệu chỉnh đo đạc.

Khi mẫu mới được nhận, handler được gọi trước PID để tiêu thụ ký tự. Vật cản dừng robot trước detector. Khi `gTurn != 0`, robot ở state quay và không kiểm tra/tiêu thụ junction khác cho đến khi quay kết thúc hoặc timeout.

Khi ngã rẽ hợp lệ, firmware gọi `handleJunction()`. Nếu chỉ số đường đi đã vượt quá độ dài mã, firmware gọi `arrived("no more code")`, dừng nhưng không kích hoạt lời chào dành riêng cho `0`. Nếu ký tự hiện tại là `0`, firmware gọi `arrived("destination")`, dừng motor và vào STOP; bắt đầu phát `welcome.mp3` ngay và ghi thời điểm bắt đầu giữ dừng. Bộ đếm thời gian không chặn duy trì STOP tối thiểu 5 giây; sau đó robot vẫn ở STOP chờ lệnh mới. Nếu ký tự là L/R, firmware tăng chỉ số và đặt `gTurn` để bắt đầu quay trái/phải. Nếu ký tự là S, firmware tăng chỉ số, reset PID rồi tiếp tục bám line thẳng.

Sau khi handleJunction xử lý xong, vòng loop gửi telemetry và kết thúc vòng hiện tại. Ở vòng kế tiếp, nếu gTurn khác 0, firmware chuyển sang nhánh updateTurn thay vì chạy PID bám line. Hàm updateTurn cho hai bánh quay ngược chiều nhau với tốc độ TURN_SPEED bằng 140. Nếu gTurn dương, bánh trái tiến và bánh phải lùi để quay phải. Nếu gTurn âm, bánh trái lùi và bánh phải tiến để quay trái.

Quá trình rẽ kết thúc theo hai điều kiện. Điều kiện thành công trong code là đã quay hơn 120 ms, mắt giữa thấy line và số mắt active không quá 2, nghĩa là xe được cho là đã bắt lại vạch. Khi đó firmware đặt gTurn về 0, đặt lại PID, xóa cờ arm và mốc căn giữa, đặt thông báo TURN OK. Điều kiện thất bại là đã quay quá 1500 ms mà vẫn chưa bắt lại được vạch. Khi đó firmware dừng động cơ, chuyển mode về STOP và đặt thông báo TURN TIMEOUT. Cơ chế timeout này giới hạn thời gian quay; chưa chứng minh detector/điều kiện bắt line phù hợp với sa bàn thực.

Khi `arrived()` được gọi, firmware dừng động cơ, đặt tốc độ hai bánh về 0, xóa trạng thái rẽ, tắt `robotEnabled`, chuyển mode về STOP, đặt thông báo ARRIVED với ID và lý do, ghi log Serial và gửi config về web. Riêng nhánh ký tự `0` phát lời chào và giữ STOP ít nhất 5 giây. Lệnh chạy mới được xử lý trong loop; khi mode không còn STOP hoặc route bị xóa, trạng thái giữ dừng sẽ kết thúc, nhưng file âm thanh đã bắt đầu có thể tiếp tục phát.

Cần hiểu rõ ý nghĩa của ký tự 0. Ký tự 0 không làm robot dừng ngay tại ngã rẽ vừa xử lý. Nó được tiêu thụ tại ngã rẽ kế tiếp. Vì vậy mã S0 nghĩa là đi thẳng qua ngã rẽ đầu tiên rồi dừng ở ngã rẽ thứ hai. Mã L0 nghĩa là rẽ trái ở ngã rẽ đầu tiên, sau đó tiếp tục bám line và dừng ở ngã rẽ kế tiếp. Mã LRLSLR0 nghĩa là lần lượt rẽ trái, rẽ phải, rẽ trái, đi thẳng, rẽ trái, rẽ phải, rồi dừng ở ngã rẽ thứ bảy. Mã RSR0 nghĩa là rẽ phải, đi thẳng, rẽ phải rồi dừng. Cách hiểu này khớp với mô tả trong giao diện: 0 là đi tới ngã rẽ kế tiếp rồi dừng.

Nếu bảng điểm đến chứa mã không kết thúc bằng 0, hàm kiểm tra sẽ loại bỏ mã đó. Tuy nhiên nếu vì lý do nào đó mã vẫn vào được firmware, handleJunction sẽ gọi arrived với lý do không còn mã khi chỉ số vượt quá độ dài. Trong thực tế, bảng mẫu gồm S0, L0, LRLSLR0 và RSR0 đều tuân thủ quy tắc.

## 10. Luồng quản lý bảng điểm đến

Bảng điểm đến là cầu nối giữa người dùng và thuật toán. Người dùng không cần biết robot đang ở ngã rẽ nào; người dùng chỉ cần nhập mã đường đi cho từng điểm đến. Firmware không cần biết ý nghĩa vật lý của điểm đến; firmware chỉ cần tiêu thụ lần lượt các ký tự khi gặp ngã rẽ.

Trong bộ nhớ, bảng là một std::map từ số nguyên sang String. Trong flash, bảng là một đối tượng JSON tại /tables.json, với khóa là ID dạng chuỗi và giá trị là mã đường đi. Ví dụ bảng mẫu có bốn mục: ID 1 là S0, ID 2 là L0, ID 3 là LRLSLR0 và ID 4 là RSR0. Khi khởi động, loadTables đọc tệp, kiểm tra từng mục và nạp vào map. Khi người dùng lưu bảng mới, saveTablesFromJson kiểm tra toàn bộ dữ liệu, ghi lại tệp và thay map trong RAM.

Hàm pathValid quy định mã hợp lệ phải không rỗng, không dài quá 90 ký tự, ký tự cuối bắt buộc là 0, ký tự 0 chỉ được xuất hiện ở cuối và các ký tự còn lại chỉ được là L, R hoặc S. Những quy tắc này được kiểm tra cả trên web lẫn trên firmware. Việc kiểm tra hai lớp giúp ngăn dữ liệu sai từ giao diện và ngăn dữ liệu sai từ một client khác.

Trên web, tab Điểm đến có một form gồm ID và mã. Khi người dùng gửi form, app.js kiểm tra ID phải lớn hơn hoặc bằng 1, chuẩn hóa mã thành chữ hoa và kiểm tra bằng biểu thức chính quy chỉ cho phép các ký tự L, R, S và kết thúc bằng 0, độ dài tối đa 90. Nếu hợp lệ, app.js cập nhật đối tượng tables trong bộ nhớ trình duyệt và vẽ lại bảng. Thay đổi này chưa được gửi xuống robot.

Khi người dùng bấm Lưu lên robot, app.js gửi PUT /api/tables với toàn bộ đối tượng tables dưới dạng JSON. Firmware nhận dữ liệu theo từng khối, ghép lại thành chuỗi, giới hạn khoảng 8 KB. Khi đã nhận đủ, firmware phân tích JSON, kiểm tra từng mục, ghi tệp /tables.json và cập nhật map. Nếu thành công, firmware trả về ok true. Nếu thất bại, firmware trả về mã 400 và thông điệp lỗi. app.js hiển thị kết quả trong dòng log của tab điểm đến. Map trong RAM được cập nhật ngay trong thao tác PUT thành công nên không cần gửi thêm lệnh tables_reload.

Nút Sửa đưa mục vào chế độ cập nhật; nếu đổi ID thì ID cũ được thay bằng ID mới, và không cho phép đổi sang ID đang được một điểm khác sử dụng. Nút Xóa chỉ xóa mục khỏi đối tượng trong trình duyệt và vẽ lại bảng; dữ liệu trên robot chỉ thay đổi khi người dùng bấm Lưu. Có thể lưu cả bảng rỗng để xóa toàn bộ điểm đến trên robot. Nút Tải lại gọi GET /api/tables để lấy bảng hiện tại từ robot và ghi đè lên bản trong trình duyệt. Nút Đi gửi lệnh goto với ID tương ứng.

Mã đường đi hiện chỉ đếm các quyết định tại ngã rẽ, không lưu khoảng cách hoặc vị trí xuất phát nằm giữa hai ngã rẽ. Vì vậy firmware hiện dừng ở điểm đến được mã hóa, nhưng chưa thể tự quay về chính xác vị trí vật lý ban đầu nếu điểm bắt đầu không phải một ngã rẽ/nút đã định danh.

Điểm kỹ thuật cần xem xét: callback AsyncWebServer hiện dùng buffer `String` static để ghép body PUT; các yêu cầu PUT đồng thời có thể dùng chung buffer. Ngoài ra, các thao tác REST cập nhật `gTables` trong ngữ cảnh async, trong khi lệnh `goto` cũng tra map từ ngữ cảnh web. Cần khóa hoặc chuyển các cập nhật/tra cứu vào một context tuần tự nếu hỗ trợ nhiều client đồng thời. Đây là rủi ro triển khai, chưa được kiểm thử tải.

## 11. Luồng phát âm thanh

Mục tiêu của phần âm thanh là phát được file MP3 lưu trên robot mà không làm ảnh hưởng đến vòng điều khiển. Nếu giải mã MP3 chạy chung trong loop, mỗi lần phát nhạc xe sẽ bị giật vì công việc giải mã chiếm thời gian đáng kể. Vì vậy firmware tách phần này thành một task FreeRTOS riêng chạy trên core 0.

Trong setup, firmware tạo AudioOutputI2S, cấu hình chân BCLK, LRC, DIN và đặt gain 0.30. Sau đó firmware tạo task audio với stack 8192 byte. Task này chạy vô hạn, mỗi vòng kiểm tra cờ gAudioNew. Khi người dùng bấm nút loa trên web, app.js gửi lệnh play với file welcome.mp3. Firmware nhận lệnh, chép tên file vào gAudioFile và bật gAudioNew. Task audio thấy cờ, tắt cờ và xử lý yêu cầu.

Task audio xây dựng đường dẫn: nếu tên file bắt đầu bằng dấu gạch chéo thì dùng nguyên, ngược lại ghép vào thư mục /audio/. Sau đó task kiểm tra file có tồn tại trong LittleFS hay không. Nếu không tồn tại, task đặt thông báo không tìm thấy file và ghi log. Nếu tồn tại, task dừng và giải phóng generator cũ nếu có, giải phóng file nguồn cũ nếu có, tạo AudioFileSourceFS mới trỏ đến file và tạo AudioGeneratorMP3 mới. Task gọi begin với output I2S đã tạo từ setup. Nếu begin thất bại, task đặt thông báo Audio begin FAILED. Nếu thành công, task đặt thông báo đang phát file. Các nguồn yêu cầu hiện có là nút Play trên web, sự kiện Wi-Fi vừa kết nối và ký tự `0` tại điểm đến.

Trong các vòng tiếp theo, nếu generator đang chạy, task gọi loop để giải mã từng khối MP3. Khi loop trả về false, nghĩa là file đã phát xong hoặc gặp lỗi, task dừng generator, giải phóng bộ nhớ, đặt thông báo Audio done. Task nghỉ 2 ms giữa các vòng để nhường CPU.

Toàn bộ quá trình này không chặn loop chính. Loop vẫn đọc cảm biến, chạy PID và gửi telemetry bình thường. Task audio chỉ chia sẻ với loop các biến gAudioFile, gAudioNew và gNotice. File MP3 phải nằm trong vùng LittleFS và phải nhỏ hơn khoảng 1.4 MB vì phân vùng LittleFS mặc định khoảng 1.5 MB. Trong dự án hiện tại, file welcome.mp3 đã có trong thư mục data/audio và được nạp cùng filesystem.

## 12. Luồng telemetry và đồng bộ giao diện

Telemetry là luồng dữ liệu từ robot lên web. Hàm sendTelemetry kiểm tra có client nào đang kết nối hay không; nếu không có thì trả về ngay để tiết kiệm CPU và băng thông. Nếu có, firmware xây dựng một chuỗi JSON gồm nhiều trường: mảng s chứa trạng thái năm cảm biến, err là sai lệch, pid là giá trị correction, l và r là tốc độ hai bánh, full là cờ cả năm mắt cùng thấy line, on là trạng thái bật tắt, mode là tên chế độ, dir là hướng lái tay, dest là ID điểm đến, step là chỉ số đường đi hiện tại, notice là thông báo, rssi là cường độ WiFi và ip là địa chỉ IP.

Hàm sendConfig xây dựng bản tin cấu hình gồm Kp, Ki, Kd, baseSpeed, maxSpeed, trạng thái bật tắt, chế độ, điểm đến và hướng lái. Bản tin này được gửi khi client mới kết nối và sau mỗi lệnh được xử lý. Nhờ vậy giao diện luôn phản ánh đúng trạng thái thực của robot, kể cả khi lệnh được gửi từ một client khác.

Tần suất telemetry được giới hạn bởi TELEMETRY_INTERVAL bằng 100 ms. Hàm telemetryThrottled kiểm tra thời gian từ lần gửi trước; nếu đã đủ 100 ms thì gọi ws.cleanupClients và sendTelemetry. Việc gọi cleanupClients định kỳ giúp dọn các kết nối WebSocket đã chết. Các nhánh chính hiện nghỉ 5 ms mỗi vòng (cộng thêm thời gian xử lý); tần số thực chưa đo. Telemetry có nhịp tối đa danh nghĩa 10 Hz.

Ở phía giao diện, app.js phân biệt hai loại bản tin. Với config, app.js cập nhật giá trị của bốn slider Kp, Ki, Kd, base và nhãn trạng thái. Biến applyingConfig được bật trong lúc cập nhật để tránh việc thay đổi giá trị slider lại kích hoạt gửi lệnh config ngược về robot. Với telemetry, app.js bật tắt đèn của năm cảm biến, cập nhật các ô Error, PID, Left, Right, cập nhật IP, RSSI, hiển thị cảnh báo khi cả năm mắt cùng thấy line, cập nhật nhãn chế độ và thông báo. Nếu đang ở chế độ PATH, dòng thông báo hiển thị thêm điểm đến và bước hiện tại. app.js cũng đưa sai lệch vào một mảng lịch sử tối đa 150 điểm và vẽ đồ thị trên canvas.

Khi người dùng kéo slider, app.js cập nhật nhãn ngay lập tức nhưng chỉ gửi lệnh config sau 150 ms kể từ lần thay đổi cuối. Cơ chế debounce này tránh gửi quá nhiều lệnh khi người dùng đang kéo. Khi lệnh config đến firmware, các giá trị được cập nhật và có hiệu lực ở vòng PID kế tiếp. Các giá trị này không được lưu vào bộ nhớ vĩnh viễn, nên sau khi khởi động lại robot sẽ trở về mặc định.

## 13. Luồng an toàn và xử lý sự cố

Có nhiều lớp bảo vệ trong hệ thống. Lớp thứ nhất là trạng thái mặc định STOP sau khi khởi động. Robot không tự chạy khi vừa cấp nguồn. Lớp thứ hai là lệnh STOP và E-STOP từ web. Lệnh đi qua queue và được xử lý ở loop; thời gian đáp ứng thực chưa đo. Cả hai hiện cùng tắt PWM, đặt các chân điều hướng về mức thấp và chuyển mode STOP; E-STOP chưa phải mạch dừng độc lập phần cứng.

Lớp thứ ba là timeout lái tay. Nếu người dùng giữ nút, app.js gửi heartbeat mỗi 250 ms. Nếu mất kết nối, heartbeat dừng và firmware tự dừng sau 600 ms. Điều này ngăn robot tiếp tục chạy khi điện thoại mất mạng hoặc tab bị đóng.

Lớp thứ tư là timeout rẽ. Trong chế độ PATH, nếu robot quay quá 1500 ms mà không bắt lại được vạch, firmware dừng động cơ và chuyển mode STOP. Điều này tránh việc quay vô hạn làm hỏng cơ cấu hoặc làm robot lao ra khỏi sa bàn.

Lớp thứ năm là kiểm tra WiFi. Nếu WiFi mất, loop kiểm tra mỗi 5 giây và gọi reconnect. Trong thời gian mất mạng, robot vẫn chạy theo chế độ hiện tại: AUTO và PATH tiếp tục vì vòng điều khiển nằm trong loop; MANUAL tự dừng vì không có heartbeat. Khi WiFi có lại, giao diện tự kết nối lại sau 1.5 giây và tiếp tục nhận telemetry.

Tuy nhiên, hệ thống vẫn còn một số điểm chưa an toàn. Nếu cả năm cảm biến đều không thấy vạch trong thời gian dài, readLine giữ sai lệch cuối và robot có thể quay vòng tại chỗ. Firmware chưa có bộ đếm thời gian mất vạch để dừng khẩn. Nếu WiFi sai hoặc không có mạng, robot không có chế độ phát WiFi riêng để người dùng truy cập. Nếu bảng điểm đến không tải được, chế độ GOTO sẽ báo không có mã. Nếu file MP3 không tồn tại, robot chỉ báo lỗi chứ không ảnh hưởng đến điều khiển. Nếu hàng đợi lệnh đầy, lệnh mới bị bỏ qua. Ngoài ra, GPIO3 và GPIO46 là các chân strapping, có thể ảnh hưởng đến chế độ nạp hoặc khởi động trong một số điều kiện.

Một chi tiết cần lưu ý là khi timeout rẽ, firmware chuyển robotMode về STOP và dừng động cơ nhưng không đặt robotEnabled về false. Do đó telemetry có thể còn báo on là true cho đến khi có lệnh STOP hoặc một lệnh arrived kế tiếp. Đây là điểm nhỏ có thể đồng bộ lại trong phiên bản sau để trạng thái trên web phản ánh chính xác hơn.

## 14. Các kịch bản chạy end-to-end

Kịch bản thứ nhất là khởi động và mở giao diện. Người dùng cấp nguồn cho robot, chờ firmware khởi tạo cảm biến, động cơ, LittleFS, bảng điểm đến, audio, WiFi và web server. Serial in ra địa chỉ IP. Người dùng mở trình duyệt, nhập IP hoặc robot.local. Trình duyệt tải index.html, style.css và app.js từ LittleFS. app.js mở WebSocket, nhận config và telemetry đầu tiên, tải bảng điểm đến qua REST. Giao diện hiển thị trạng thái STOP, năm đèn cảm biến theo môi trường thực tế và các thông số mặc định.

Kịch bản thứ hai là chạy AUTO và chỉnh PID. Người dùng bấm START. Lệnh đi vào hàng đợi, loop chuyển mode AUTO và bật robotEnabled. Robot đọc cảm biến, tính sai lệch, chạy PID và bám vạch. Trên web, năm đèn cảm biến nhấp nháy theo vạch, đồ thị sai lệch vẽ theo thời gian, tốc độ hai bánh thay đổi liên tục. Người dùng kéo Kp lên cao, gửi lệnh config, robot phản ứng mạnh hơn và có thể dao động. Người dùng giảm Kp hoặc tăng Kd để ổn định lại. Việc chỉnh diễn ra trực tiếp, không cần nạp lại firmware.

Kịch bản thứ ba là lái tay. Người dùng giữ nút tiến trên D-pad hoặc giữ phím mũi tên. app.js gửi lệnh drive và heartbeat. Firmware chuyển mode MANUAL, tính tốc độ theo hướng và chạy. Khi người dùng thả nút, app.js gửi stop. Nếu người dùng giữ hai nút, ví dụ tiến và trái, app.js gửi forward_left và robot chạy cua trái. Nếu kết nối bị mất, sau 600 ms firmware tự dừng.

Kịch bản thứ tư là kịch bản chức năng mong đợi cho mã `LRLSLR0`, không phải kết quả đã chứng minh trên phần cứng. Người dùng mở tab Điểm đến, bấm Đi ở ID 3. Web gửi `goto`; firmware tra mã, đặt MODE_PATH, `gPathIdx = 0`, `gDestId = 3`. Mỗi junction được detector chấp nhận sẽ tiêu thụ đúng một ký tự: L/R khởi chạy state quay; S reset PID và đi thẳng; ký tự cuối 0 dừng, phát `welcome.mp3`, giữ STOP ít nhất 5 giây. Người dùng gần đây đã báo robot chưa nhận junction; cho đến khi kiểm tra được log S1–S5 và chạy lặp lại thành công, không trình bày chuỗi này như kết quả thực nghiệm.

Kịch bản thứ năm là sửa bảng điểm đến. Người dùng thêm/sửa/xóa trong bản local trên web; sau khi bấm Lưu lên robot, app.js gửi PUT `/api/tables`. Firmware xác thực, ghi `/tables.json`, cập nhật `gTables` và trả kết quả. Giao diện không cần gửi `tables_reload` sau PUT thành công. Có thể lưu object rỗng để xóa toàn bộ điểm. Cần thử luồng này trên board trước khi ghi nhận là kiểm thử nghiệm thu.

Kịch bản thứ sáu là phát âm thanh. Người dùng bấm nút phát welcome.mp3. app.js gửi lệnh play. Firmware đặt gAudioFile và bật cờ. Task audio trên core 0 kiểm tra file, tạo generator và phát qua I2S. Trong lúc đó, loop vẫn bám line và gửi telemetry. Khi file kết thúc, task audio giải phóng bộ nhớ và đặt thông báo Audio done.

Kịch bản thứ bảy là dừng khẩn cấp. Người dùng bấm E-STOP. Lệnh đi vào hàng đợi, loop tắt PWM, đặt các chân điều hướng về mức thấp, chuyển mode STOP và gửi config. Robot dừng. Nếu đang ở chế độ PATH, đường đi và chỉ số bị xóa. Người dùng có thể bấm START hoặc chọn điểm đến mới để chạy lại.

## 15. Quy trình biên dịch, nạp và vận hành

Quy trình phát triển sử dụng PlatformIO. Trước khi biên dịch, cần mở tệp platformio.ini và kiểm tra các thư viện phụ thuộc. Trong mã nguồn, cần sửa WIFI_SSID và WIFI_PASS cho đúng mạng WiFi 2.4 GHz sẽ dùng. Nếu cảm biến báo ngược, đổi LINE_ON từ HIGH sang LOW. Nếu bánh xe quay ngược, đảo dây động cơ hoặc đảo chân IN trong firmware.

Lệnh build firmware là pio run. Lệnh nạp firmware là pio run -t upload. Lệnh nạp giao diện web, bảng điểm đến và âm thanh là pio run -t uploadfs. Lệnh mở Serial Monitor là pio device monitor với tốc độ 115200. Trên Windows, nếu lệnh pio chưa có trong PATH, có thể thêm thư mục Scripts của PlatformIO vào biến môi trường trước khi chạy.

Thứ tự nạp hợp lý là nạp firmware trước, sau đó nạp filesystem. Nạp filesystem sẽ ghi đè toàn bộ vùng LittleFS, vì vậy nếu đã chỉnh bảng điểm đến trên robot và muốn giữ, cần sao lưu hoặc chấp nhận nạp lại bảng mẫu. Khi chỉ thay đổi giao diện hoặc thêm file MP3, chỉ cần nạp lại filesystem. Khi chỉ thay đổi logic, chỉ cần nạp firmware.

Sau khi nạp, mở Serial Monitor để xem log khởi động. Log sẽ cho biết LittleFS có mount được không, bảng điểm đến nạp được bao nhiêu mục, WiFi có tìm thấy mạng mục tiêu không, địa chỉ IP, mDNS có thành công không và web server đã chạy chưa. Nếu WiFi không kết nối, log sẽ in lý do như không thấy tên mạng, sai mật khẩu hoặc mất kết nối. Khi robot hoạt động, log in trạng thái cảm biến, sai lệch, PID, tốc độ hai bánh, chế độ và các sự kiện ngã rẽ.

Quy trình vận hành demo bắt đầu bằng việc kiểm tra pin, kê bánh khỏi mặt bàn khi kiểm tra chiều motor, kiểm tra điện áp ECHO trước khi nối GPIO, đặt robot lên track và xác nhận cảm biến qua telemetry. Sau khi mở web, thử STOP và manual ở tốc độ thấp. Chỉ demo PATH nếu detector đã được kiểm chứng trên track; hiện robot được báo chưa phát hiện junction, vì vậy ưu tiên thu log pattern thay vì tuyên bố điều hướng đã đạt. Cuối cùng dùng STOP/E-STOP; không coi E-STOP trên web là phần cứng độc lập.

## 16. Kết quả đạt được, hạn chế và hướng phát triển

### Trạng thái có bằng chứng

- Firmware đã được build bằng PlatformIO sau cập nhật detector ưu tiên trước PID: **SUCCESS**. Build xác nhận biên dịch/link, không xác nhận hoạt động phần cứng.
- Trong source có các module PID, bốn mode, WebSocket/REST, bảng điểm LittleFS, MP3 task, NTP/TM1637 và HY-SRF05 obstacle pause.
- Chu kỳ nghỉ được cấu hình 5 ms; telemetry được throttle 100 ms. Tần số/độ trễ thực chưa đo.
- Người dùng báo robot vẫn không phát hiện ngã rẽ. Đây là lỗi runtime đang mở; chưa có log sensor để xác định polarity, pattern, trạng thái arm, tốc độ hoặc vấn đề track.

### Hạn chế và việc cần đo

1. **Detector junction chưa được xác nhận hoạt động:** kiểm tra cấu hình `LINE_ON`, quan sát S1–S5 tại nền/vạch/ngã rẽ, xác nhận pattern `1...0...1`, ghi `gJunctionArmed`, thời gian 30 ms và mode. Chỉ sau khi có dữ liệu mới chọn sửa polarity, hình học sensor hoặc ngưỡng.
2. **Không encoder:** PID điều khiển sai lệch vị trí line, không đo tốc độ thực từng bánh; độ lệch cơ khí/động cơ chưa được định lượng.
3. **Mất line chưa có fail-safe timeout:** khi không sensor nào thấy line, `readLine()` giữ `lastError`, có thể khiến robot tiếp tục bẻ theo sai số cũ.
4. **Không timeout toàn tuyến:** timeout 1500 ms chỉ áp dụng cho state quay; PATH có thể chạy vô hạn nếu không thấy junction.
5. **Điểm dừng và sai số hành trình chưa đo:** `0` dừng tại pattern được chấp nhận; chưa bù quán tính hoặc đo sai số vị trí.
6. **Không tự quay về gốc:** mã chỉ chứa lệnh tại junction, không có tọa độ/khoảng cách; chưa thể đảm bảo quay về vị trí xuất phát giữa các nút.
7. **Obstacle monitor chưa xác nhận phần cứng:** ngưỡng cấu hình <10 cm dừng, ≥15 cm xóa obstacle; hai lần liên tiếp không echo cũng xóa cờ. ECHO GPIO48 phải không vượt 3.3 V. Cảm biến chỉ bảo vệ vùng phía trước và độ tin cậy tùy bề mặt/lắp đặt.
8. **NTP cần Internet** để đồng bộ; độ chính xác sau ngắt Wi-Fi chưa đo.
9. **Web chưa có xác thực/AP fallback:** người dùng cùng mạng có thể gửi lệnh; Wi-Fi STA không có phương án độc lập nếu cấu hình sai.
10. **IR/encoder chưa tích hợp.** PID hiện tại không được lưu bền vững qua khởi động.
11. **Chân strapping:** GPIO3, GPIO46 và GPIO45 cần được thử boot/nạp trên board đúng với wiring cuối.
12. **Concurrent REST risk:** buffer body PUT là static và bảng map có thể được truy cập từ callback async; chưa thử nhiều client đồng thời.

### Thứ tự ưu tiên phát triển

1. Thu log sensor và sửa/hiệu chỉnh detection junction; chạy bài test trên track ở tốc độ thấp.
2. Đo lặp tỷ lệ nhận junction, rẽ đúng, đến đúng điểm, thời gian hành trình và sai số dừng.
3. Thêm fail-safe mất line và timeout toàn tuyến.
4. Thêm encoder và đo tốc độ hai bánh; lưu cấu hình PID nếu cần.
5. Xác định topology/điểm gốc và đo khoảng cách trước khi thiết kế return-to-origin.
6. Sau đó mới mở rộng IR, AP fallback, xác thực web, pin monitor hoặc OTA.

Không có phép đo runtime nào được suy ra từ kích thước firmware hoặc build thành công. Các kết quả định lượng cho báo cáo/paper cần thu thập trên track và ghi rõ setup, số lần lặp cùng điều kiện môi trường.

## 17. Kết luận về flow tổng thể

Flow toàn bộ dự án có thể được hiểu như một chuỗi khép kín gồm bốn tầng. Tầng thứ nhất là tầng vật lý: pin cấp nguồn, cảm biến đọc vạch, L298N nhận PWM và MAX98357 nhận I2S. Tầng thứ hai là tầng điều khiển: ESP32-S3 đọc năm cảm biến, tính sai lệch, chạy PID, nhận diện ngã rẽ và điều khiển hai bánh. Tầng thứ ba là tầng giao tiếp: WiFi, web server, WebSocket và REST API kết nối robot với trình duyệt. Tầng thứ tư là tầng quản lý: bảng điểm đến, mã đường đi, telemetry và các thông báo trạng thái giúp người dùng theo dõi và thay đổi hành vi của robot mà không cần sửa firmware.

Thiết kế tách module và tách task thể hiện rõ trách nhiệm của từng khối: route code quy định hành động tại junction, PID điều chỉnh theo line, web gửi lệnh và quan sát telemetry, audio chạy trong task riêng. Tuy nhiên, việc detector junction chưa được xác nhận trên robot là vấn đề runtime đang mở; kết luận paper cần dựa trên số liệu thực nghiệm chứ không chỉ sơ đồ kiến trúc hoặc build. Hướng mở rộng gồm xử lý mất line, encoder, timeout toàn tuyến, định nghĩa topology/origin, xác thực web và kiểm chứng obstacle monitor.

Tóm lại, toàn bộ hành trình của dự án đi từ bài toán giao hàng thủ công, đến ý tưởng robot tự hành trên sa bàn vạch kẻ, đến thiết kế phần cứng với ESP32-S3 và năm mắt dò line, đến firmware với PID và máy trạng thái đường đi, đến giao diện web realtime và cuối cùng là quy trình nạp, vận hành, demo. Mỗi thành phần đều có đầu vào, đầu ra và vai trò rõ ràng, tạo thành một hệ thống hoàn chỉnh có thể quan sát, điều khiển và mở rộng.
