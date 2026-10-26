/*
 * ============================================================================
 * GIẢI THÍCH CHI TIẾT TỪNG TRƯỜNG TRONG struct input_dev
 * ============================================================================
 *
 * ----------------------------------------------------------------------------
 * NHÓM 1: ĐỊNH DANH & VỊ TRÍ PHẦN CỨNG (name, phys, uniq, id)
 * ----------------------------------------------------------------------------
 * 1. const char *name:
 *    - Khái niệm: Tên chuỗi ký tự mô tả thiết bị mà người dùng có thể đọc
 *      hiểu (human-readable string), ví dụ: "Nintendo Wii Nunchuk", "Logitech USB Mouse".
 *    - Tại sao Kernel cần: Xuất hiện trong /proc/bus/input/devices, sysfs,
 *      công cụ evtest, và giúp udev hoặc libinput hiển thị đúng tên thiết bị
 *      cho người dùng.
 *    - Khi nào set / bỏ qua: NÊN LUÔN LUÔN SET. Nếu bỏ qua, tên sẽ bị trống
 *      hoặc NULL, hệ thống và người dùng không biết đây là thiết bị gì.
 *
 * 2. const char *phys:
 *    - Khái niệm: Đường dẫn vật lý (physical topology path) trong cây phần cứng,
 *      ví dụ: "i2c-1/1-0052/input0" hoặc "usb-0000:00:14.0-1/input0".
 *    - Tại sao Kernel cần: Giúp hệ thống phân biệt 2 thiết bị giống hệt nhau
 *      khi cắm ở 2 cổng khác nhau (ví dụ: cắm 2 tay cầm giống hệt vào 2 cổng USB/I2C khác nhau).
 *    - Khi nào set / bỏ qua: CÓ THỂ BỎ QUA với các driver vi điều khiển/I2C
 *      nhúng đơn giản. Thường bắt buộc đối với driver USB hoặc các driver phức tạp.
 *
 * 3. const char *uniq:
 *    - Khái niệm: Mã định danh duy nhất (Unique Serial/MAC Address) của thiết bị
 *      vật lý cụ thể (ví dụ: số Serial Number của tay cầm, địa chỉ MAC Bluetooth).
 *    - Tại sao Kernel cần: Giúp hệ thống nhận ra đúng thiết bị cụ thể đó dù nó
 *      được rút ra cắm lại ở một cổng khác.
 *    - Khi nào set / bỏ qua: BỎ QUA trừ khi thiết bị có chip EEPROM lưu số serial
 *      riêng hoặc kết nối qua Bluetooth/USB có Serial Number.
 *
 * 4. struct input_id id:
 *    - Khái niệm: Chứa bộ 4 thông số phần cứng: bustype (BUS_I2C, BUS_USB,...),
 *      vendor, product, version.
 *    - Tại sao Kernel cần: udev và các thư viện đồ họa (như libinput trong Wayland)
 *      dùng thông số này để áp dụng các cấu hình riêng biệt hoặc sửa lỗi (quirks)
 *      riêng cho nhà sản xuất đó.
 *    - Khi nào set / bỏ qua: CẦN SET ÍT NHẤT TRƯỜNG bustype (ví dụ: input_dev->id.bustype = BUS_I2C;).
 *      Các trường vendor/product/version có thể bỏ qua (để = 0) nếu không có mã ID chuẩn.
 *
 * ----------------------------------------------------------------------------
 * NHÓM 2: THUỘC TÍNH ĐẶC BIỆT (propbit)
 * ----------------------------------------------------------------------------
 * 5. unsigned long propbit[...]:
 *    - Khái niệm: Bitmap mô tả tính chất vật lý của thiết bị
 *      (INPUT_PROP_DIRECT, INPUT_PROP_POINTER, INPUT_PROP_ACCELEROMETER...).
 *    - Tại sao Kernel cần: Giúp Userspace hiểu bản chất tương tác.
 *      + Ví dụ: INPUT_PROP_DIRECT báo cho Wayland biết đây là màn hình cảm ứng
 *        (ngón tay chạm thẳng vào pixel trên màn hình).
 *      + INPUT_PROP_POINTER báo đây là touchpad (di chuyển trỏ chuột tương đối).
 *    - Khi nào set / bỏ qua: BỎ QUA đối với các thiết bị tay cầm/nút bấm thông
 *      thường (như Nunchuk). Chỉ bắt buộc khi làm Touchpad, Touchscreen đa điểm.
 *
 * ----------------------------------------------------------------------------
 * NHÓM 3: KHAI BÁO KHẢ NĂNG (CAPABILITIES BITMAPS)
 * (Lưu ý: Hàm devm_input_allocate_device() đã tự động xóa toàn bộ các bit về 0)
 * ----------------------------------------------------------------------------
 * 6. unsigned long evbit[...] (Event types bitmask):
 *    - Khái niệm: Mặt nạ bit quy định những loại sự kiện lớn mà thiết bị hỗ trợ
 *      (EV_KEY cho nút bấm, EV_ABS cho joystick, EV_REL cho chuột...).
 *    - Tại sao Kernel cần: Đây là bộ lọc cổng (Gatekeeper) của Input Core. Khi
 *      bạn gọi hàm input_event(dev, type, code, val), Kernel sẽ kiểm tra bit của
 *      type trong evbit. Nếu bit này bằng 0, Kernel lập tức vứt bỏ event, không
 *      gửi lên userspace.
 *    - Khi nào set / bỏ qua: BẮT BUỘC PHẢI CÓ.
 *      Tuy nhiên: Nếu bạn dùng các hàm trợ giúp như input_set_capability(dev, EV_KEY, BTN_Z)
 *      hoặc input_set_abs_params(), Kernel sẽ TỰ ĐỘNG BẬT bit trong evbit cho bạn,
 *      không cần tự bật bằng tay.
 *
 * 7. Các mảng bit chi tiết: keybit, relbit, absbit,...:
 *    - Khái niệm: Khai báo chi tiết những mã cụ thể nào sẽ xuất hiện:
 *      + keybit: Danh sách nút bấm (BTN_Z, BTN_C, KEY_ENTER...).
 *      + absbit: Danh sách các trục tọa độ tuyệt đối (ABS_X, ABS_Y của cần gạt joystick).
 *      + relbit: Trục tương đối (chuột lăn REL_WHEEL, di chuột REL_X).
 *      + ledbit / sndbit / ffbit: Đèn bàn phím, tiếng chuông beep, rung (Force Feedback).
 *      + swbit: Công tắc nhị phân (gập mở màn hình laptop SW_LID, cắm tai nghe).
 *    - Tại sao Kernel cần: Khi userspace chạy lệnh ioctl(EVIOCGBIT), Kernel trả về
 *      danh sách này để ứng dụng biết thiết bị có bao nhiêu nút, cần gạt gì để
 *      vẽ giao diện hoặc cấu hình game.
 *    - Khi nào set / bỏ qua:
 *      + Thiết bị có tính năng nào thì BẮT BUỘC SET tính năng đó (thường thông qua
 *        input_set_capability hoặc input_set_abs_params).
 *      + Những tính năng thiết bị không có (ví dụ Nunchuk không có đèn LED, không
 *        có âm thanh) thì BỎ QUA HOÀN TOÀN.
 *
 * ----------------------------------------------------------------------------
 * NHÓM 4: TỐI ƯU BỘ ĐỆM & BẢNG MÃ PHÍM
 * ----------------------------------------------------------------------------
 * 8. unsigned int hint_events_per_packet:
 *    - Khái niệm: số lượng sự kiện tối đa mà driver sẽ gửi trong một nhịp
 *      (giữa 2 lần gọi input_sync()). Tức là số lượng sự kiện có thể sinh ra tối
 *		đa trong 1 lần gửi của input (ví dụ nhấn tất cả nút trên device 1 lúc)
 *    - Tại sao Kernel cần: Kernel dùng con số này để cấp phát kích thước hàng đợi
 *      vòng (ring buffer) trong evdev, tránh việc buffer quá nhỏ dẫn đến tràn bộ
 *      nhớ (drop packet) khi thiết bị gửi dữ liệu ồ ạt.
 *    - Khi nào set / bỏ qua: CÓ THỂ BỎ QUA (để mặc định = 0). Kernel sẽ tự dùng
 *      kích thước ước lượng chuẩn (~32 events). Chỉ cần set với thiết bị gửi khối
 *      lượng cực lớn cùng lúc (như màn hình cảm ứng 10 ngón tay).
 *
 * 9. keycodemax, keycodesize, void *keycode:
 *    - Khái niệm: Bộ 3 quản lý bảng ánh xạ phím (Keymap Table) từ mã quét phần
 *      cứng (Scancode) sang mã phím Linux (KEY_*).
 *    - Tại sao Kernel cần: Cho phép người dùng ở userspace dùng lệnh setkeycodes
 *      để remap/đổi chức năng nút bấm mà không cần sửa code driver.
 *    - Khi nào set / bỏ qua: BỎ QUA đối với thiết bị có số nút cố định (như Nunchuk
 *      chỉ có nút C và Z gắn chết). Chỉ cần set khi bạn viết driver cho bàn phím
 *      máy tính ma trận (matrix keyboard) hoặc remote hồng ngoại.
 *
 * ----------------------------------------------------------------------------
 * NHÓM 5: TÙY BIẾN MÃ PHÍM (Keycode Callbacks)
 * ----------------------------------------------------------------------------
 * 10. int (*setkeycode)(...) và int (*getkeycode)(...):
 *     - Khái niệm: Con trỏ hàm cho phép đọc và ghi đè bảng ánh xạ phím
 *       (Keymap) từ mã quét phần cứng (scancode) sang mã phím Linux (KEY_*).
 *     - Tại sao Kernel cần: Khi userspace chạy lệnh evtest hoặc gọi ioctl
 *       (EVIOCGKEYCODE / EVIOCSKEYCODE) để đổi chức năng một phím, Kernel
 *       sẽ gọi 2 hàm này.
 *     - Khi nào set / bỏ qua: HẦU NHƯ LUÔN BỎ QUA. Kernel đã có sẵn hàm mặc
 *       định input_default_getkeycode và input_default_setkeycode. Chỉ cần
 *       tự viết khi phần cứng có bảng mã quét đặc thù (như remote hồng ngoại).
 *
 * ----------------------------------------------------------------------------
 * NHÓM 6: PHẢN HỒI LỰC & POLLING (Force Feedback & Poller)
 * ----------------------------------------------------------------------------
 * 11. struct ff_device *ff:
 *     - Khái niệm: Cấu trúc quản lý hiệu ứng rung / phản hồi lực (Force
 *       Feedback / Haptic Rumble) của tay cầm game (như rung tay cầm Xbox, PS).
 *     - Tại sao Kernel cần: Lưu danh sách các hiệu ứng rung (độ mạnh, thời
 *       gian rung, kiểu sóng rung) mà game/ứng dụng yêu cầu driver thực hiện.
 *     - Khi nào set / bỏ qua: BỎ QUA nếu thiết bị không có motor rung. Nếu có
 *       rung, driver không gán trực tiếp mà gọi hàm trợ giúp input_ff_create().
 *
 * 12. struct input_dev_poller *poller:
 *     - Khái niệm: Đối tượng quản lý luồng hỏi vòng (polling thread/workqueue)
 *       định kỳ của thiết bị.
 *     - Tại sao Kernel cần: Chứa con trỏ hàm đọc dữ liệu định kỳ và bộ đếm
 *       chu kỳ (interval, ví dụ 50ms) để tự động đánh thức driver đọc I2C/SPI
 *       mà không cần chân ngắt phần cứng.
 *     - Khi nào set / bỏ qua: KHÔNG GÁN TRỰC TIẾP. Khi dùng polling (như
 *       Nunchuk), chỉ cần gọi input_setup_polling(input_dev, nunchuk_poll_fn)
 *       và input_set_poll_interval(input_dev, 50).
 *
 * ----------------------------------------------------------------------------
 * NHÓM 7: TỰ ĐỘNG LẶP PHÍM KHI GIỮ (Autorepeat)
 * ----------------------------------------------------------------------------
 * 13. unsigned int repeat_key, struct timer_list timer, int rep[REP_CNT]:
 *     - Khái niệm: Cơ chế tự động lặp phím khi giữ (như khi đè giữ phím A trên
 *       bàn phím thì chữ 'a' liên tục xuất hiện).
 *       + rep[REP_DELAY]: Thời gian giữ trước khi bắt đầu lặp (mặc định 250ms).
 *       + rep[REP_PERIOD]: Tốc độ lặp lại phím (mặc định 33ms/lần).
 *       + timer: Bộ định thời Kernel Timer chạy ngầm đếm thời gian lặp.
 *       + repeat_key: Lưu mã phím hiện đang được đè giữ.
 *     - Tại sao Kernel cần: Quản lý việc phát sinh sự kiện lặp phím ảo gửi lên
 *       userspace mà phần cứng không cần gửi liên tục.
 *     - Khi nào set / bỏ qua: BỎ QUA. Kernel tự động kích hoạt cho các thiết bị
 *       có EV_KEY. Nếu không muốn lặp (như tay cầm game), gọi:
 *       clear_bit(EV_REP, input_dev->evbit).
 *
 * ----------------------------------------------------------------------------
 * NHÓM 8: ĐA ĐIỂM & THÔNG SỐ TRỤC (Multi-touch & Absinfo)
 * ----------------------------------------------------------------------------
 * 14. struct input_mt *mt:
 *     - Khái niệm: Cấu trúc quản lý giao thức cảm ứng đa điểm (Multi-touch).
 *     - Tại sao Kernel cần: Theo dõi từng ngón tay riêng biệt (tracking ID,
 *       tọa độ X/Y từng ngón, ngón nào vừa chạm xuống, ngón nào vừa nhấc lên).
 *     - Khi nào set / bỏ qua: BỎ QUA với Nunchuk/Joystick thông thường. Chỉ dùng
 *       khi viết driver màn hình cảm ứng điện dung, touchpad (qua input_mt_init_slots).
 *
 * 15. struct input_absinfo *absinfo:
 *     - Khái niệm: Mảng chứa thông số min, max, fuzz, flat, resolution của tất
 *       cả các trục tuyệt đối (ABS_X, ABS_Y...).
 *     - Tại sao Kernel cần: Lưu bảng thông số căn chỉnh (calibration) của các
 *       trục để trả về cho userspace qua ioctl(EVIOCGABS).
 *     - Khi nào set / bỏ qua: KHÔNG CẤP PHÁT BẰNG TAY. Hàm input_set_abs_params()
 *       đã tự động cấp phát và điền dữ liệu vào mảng này.
 *
 * ----------------------------------------------------------------------------
 * NHÓM 9: BITMAP LƯU TRẠNG THÁI HIỆN TẠI (Current State Bitmaps)
 * ----------------------------------------------------------------------------
 * 16. unsigned long key[...], led[...], snd[...], sw[...]:
 *     - Khái niệm: Lưu trạng thái tức thời (đang bật hay tắt, đang nhấn hay
 *       nhả) của phím, đèn led, âm thanh, công tắc. Phân biệt với keybit:
 *       keybit là danh sách phím CÓ THỂ CÓ, còn key là phím NGAY LÚC NÀY đang
 *       được nhấn hay không.
 *     - Tại sao Kernel cần: Khi có ứng dụng mới mở /dev/input/eventX, nó có thể
 *       truy vấn ngay trạng thái hiện tại mà không cần đợi người dùng bấm lại.
 *     - Khi nào set / bỏ qua: KHÔNG BAO GIỜ SET BẰNG TAY. Mỗi khi driver gọi
 *       input_report_key(dev, BTN_Z, 1), Input Core tự động cập nhật mảng này.
 *
 * ----------------------------------------------------------------------------
 * NHÓM 10: CALLBACK ĐIỀU KHIỂN VÒNG ĐỜI THIẾT BỊ (Driver Callbacks)
 * ----------------------------------------------------------------------------
 * 17. int (*open)(...), void (*close)(...):
 *     - Khái niệm: Callback được Kernel gọi khi có ứng dụng mở (open) hoặc
 *       tất cả ứng dụng đóng (close) file /dev/input/eventX.
 *     - Tại sao Kernel cần: Quản lý năng lượng (Power Saving). Khi không có
 *       ai mở /dev/input, Kernel gọi close() để driver cho phần cứng ngủ,
 *       tiết kiệm điện. Khi có ứng dụng mở, Kernel gọi open() để đánh thức.
 *     - Khi nào set / bỏ qua: Có thể bỏ qua với driver đơn giản. Nên set nếu
 *       muốn tiết kiệm điện năng hoặc chỉ bật polling khi có app lắng nghe.
 *
 * 18. int (*event)(struct input_dev *dev, unsigned int type, unsigned int code, int value):
 *     - Khái niệm: Callback nhận sự kiện ngược từ Userspace gửi xuống phần cứng.
 *     - Tại sao Kernel cần: Dùng khi userspace muốn điều khiển phần cứng (ví dụ:
 *       bật đèn CapsLock/NumLock qua EV_LED, phát tiếng bíp qua EV_SND, rung EV_FF).
 *     - Khi nào set / bỏ qua: BỎ QUA với thiết bị chỉ gửi dữ liệu lên (như
 *       Nunchuk không có đèn led hay còi báo cần nhận lệnh từ userspace).
 *
 * ----------------------------------------------------------------------------
 * NHÓM 11: CÁC TRƯỜNG NỘI BỘ DO KERNEL TỰ QUẢN LÝ (Private / Internal Core)
 * ----------------------------------------------------------------------------
 * 19. Các trường từ dòng 190 đến 212:
 *     (grab, event_lock, mutex, users, going_away, dev, h_list, node,
 *      num_vals, max_vals, vals, devres_managed, timestamp, inhibited)
 *     - Khái niệm: Cơ sở hạ tầng nội bộ của Input Subsystem để điều phối đa
 *       luồng, khóa đồng bộ (mutex/spinlock), quản lý bộ nhớ đệm tạm thời
 *       (vals), kết nối danh sách handler và xuất thiết bị ra sysfs.
 *     - Tại sao Kernel cần: Đảm bảo tính toàn vẹn dữ liệu đa nhiệm giữa driver,
 *       kernel core và các tiến trình userspace.
 *     - Khi nào set / bỏ qua: TUYỆT ĐỐI BỎ QUA (Không được phép can thiệp).
 *       Tất cả các trường này do Input Core tự động khởi tạo và điều khiển.
 *
 * ============================================================================
 * VỚI CON NUNCHUK, CHỈ CẦN QUAN TÂM ĐÚNG 3 MỤC:
 * 1. name và id.bustype (Định danh).
 * 2. input_set_capability(..., EV_KEY, BTN_Z / BTN_C) (Tự động set evbit và keybit).
 * 3. input_set_abs_params(..., ABS_X / ABS_Y, 0, 255, ...) (Tự động set evbit và absbit).
 * ============================================================================
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/module.h>

#define pr_fmt(fmt) fmt "\n"

int nunchuk_probe(struct i2c_client *client);
void nunchuk_remove(struct i2c_client *client);
void nunchuk_polling(struct input_dev *dev);

const struct of_device_id nunchuk_of_match_table[] = {
    {.compatible = "nintendo,nunchuk"},
    {},
};

static struct i2c_driver nunchuk_i2c_driver = {
    .probe = nunchuk_probe,
    .remove = nunchuk_remove,
    .driver = {
	.name = "nunchuk",
	.of_match_table = nunchuk_of_match_table,
    },
};
module_i2c_driver(nunchuk_i2c_driver);

struct nunchuk_drvdata {
	const char *name;
	struct i2c_client *nunchuk_i2c_client;
	struct input_dev *nunchuk_input_dev;
};

int nunchuk_probe(struct i2c_client *client) {
	pr_info("%s device '%s'", __func__, client->name);

	int ret;
	u8 buf_setup[2];

	struct nunchuk_drvdata *nunchuk_drvdata;
	struct input_dev *input_dev;

	nunchuk_drvdata = devm_kzalloc(&client->dev, sizeof(struct nunchuk_drvdata), GFP_KERNEL);

	// ============== thiết lập và mô tả device trong linux =============
	input_dev = devm_input_allocate_device(&client->dev);
	if (!input_dev) {
		pr_info("%s - cannot allocate input_dev\n", __func__);
	}

	nunchuk_drvdata->nunchuk_i2c_client = client;
	nunchuk_drvdata->nunchuk_input_dev = input_dev;

	input_set_drvdata(input_dev, client);
	i2c_set_clientdata(client, nunchuk_drvdata);

	// đặt tên cho nunchuk trong /proc/bus/input/devices
	input_dev->name = "nunchuk";
	nunchuk_drvdata->name = input_dev->name;

	// mô tả thuộc bus nào
	input_dev->id.bustype = BUS_I2C;

	// khai báo khả năng của các nút nhấn mà input hỗ trợ
	input_set_capability(input_dev, EV_KEY, BTN_Z);
	input_set_capability(input_dev, EV_KEY, BTN_C);
	input_set_abs_params(input_dev, ABS_X, 0, 255, 1, 1); // fuzz: giá trị mới lệch 4 so với cũ thì mới ghi nhận, flat: giá trị quanh khoảng (-flat, 0, flat) đều được coi là 0
	input_set_abs_params(input_dev, ABS_Y, 0, 255, 4, 8);
	input_set_abs_params(input_dev, ABS_Z, 0, 255, 4, 8);

	// cấu hình polling để đọc giá trị nunchuk (vì nunchuk không có ngắt)
	input_setup_polling(input_dev, nunchuk_polling);
	input_set_poll_interval(input_dev, 50);

	// ============== cấu hình thanh ghi cho phần cứng =============
	buf_setup[0] = 0xF0;
	buf_setup[1] = 0x55;
	i2c_smbus_write_byte_data(client, buf_setup[0], buf_setup[1]);
	udelay(1000);
	buf_setup[0] = 0xFB;
	buf_setup[1] = 0x00;
	i2c_smbus_write_byte_data(client, buf_setup[0], buf_setup[1]);
	udelay(1000);

	// ============== Đăng ký với linux =======================
	ret = input_register_device(input_dev);
	if (ret < 0) {
		pr_info("%s failed to register input device", __func__);
		return ret;
	}

	return 0;
}

void nunchuk_remove(struct i2c_client *client) {
	pr_info("%s", __func__);
}

void nunchuk_polling(struct input_dev *dev) {
	struct i2c_client *i2c_client = input_get_drvdata(dev);

	u8 buf[6];
	u8 buf_send;

	buf_send = 0x00;
	i2c_master_send(i2c_client, &buf_send, 1);
	mdelay(10);

	i2c_master_recv(i2c_client, buf, 6);

	bool btn_z = buf[5] & BIT(0) ? 0 : 1;
	bool btn_c = buf[5] & BIT(1) ? 0 : 1;
	u16 accel_x = (buf[2] << 2) | ((buf[5] >> 2) & (BIT(1) | BIT(0)));

	input_report_key(dev, BTN_Z, btn_z);
	input_report_key(dev, BTN_C, btn_c);
	input_report_abs(dev, ABS_X, buf[0]);
	// input_report_key(dev, ABS_X, int value);
	// input_report_key(dev, ABS_Y, int value);

	input_sync(dev);
}

MODULE_LICENSE("GPL");
