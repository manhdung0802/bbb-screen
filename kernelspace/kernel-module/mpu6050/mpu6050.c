#include <linux/buffer_head.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/iio/buffer.h>
#include <linux/iio/iio.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#define MPU6050_ACCEL_CHANNEL(index, reg, axis)                   \
	{                                                         \
	    .type = IIO_ACCEL,                                    \
	    .modified = 1,                                        \
	    .channel2 = IIO_MOD_##axis,                           \
	    .info_mask_separate = BIT(IIO_CHAN_INFO_RAW),         \
	    .info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE), \
	    .scan_index = index,                                  \
	    .scan_type = {                                        \
		.sign = 's',                                      \
		.realbits = 16,                                   \
		.storagebits = 16,                                \
		.shift = 0,                                       \
		.endianness = IIO_BE,                             \
	    },                                                    \
	    .output = 0,                                          \
	    .address = reg,                                       \
	}

static int mpu_probe(struct i2c_client *client);
static void mpu_remove(struct i2c_client *client);
int mpu_set_trigger_state(struct iio_trigger *trig, bool state);

static const struct of_device_id mpu_of_ids[] = {
    {.compatible = "mpu6050"},
    {},
};
MODULE_DEVICE_TABLE(of, mpu_of_ids);

static struct i2c_driver mpu_driver = {
    .driver = {
	.name = "mpu6050",
	.of_match_table = mpu_of_ids,
    },
    .probe = mpu_probe,
    .remove = mpu_remove,
};
module_i2c_driver(mpu_driver);

struct mpu_drvdata {
	struct i2c_client *mpu_i2c_client;
	struct iio_dev *mpu_iio_dev;
	struct iio_trigger *mpu_iio_trigger;
};

int mpu_read_raw(struct iio_dev *indio_dev,
		 struct iio_chan_spec const *chan,
		 int *val,
		 int *val2,
		 long mask) {

	int ret;
	struct mpu_drvdata *mpu_drvdata = iio_priv(indio_dev);

	// mask: các cờ  IIO_CHAN_INFO_*
	// cần chia switch case cho từng loại type trong kênh iio_chan_spec
	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		// i2c smbus trả byte thấp trước mà mpu6050 trả byte cao trước nên cần dùng
		// i2c_smbus_read_word_swapped để đảo cho đúng thứ tự
		ret = i2c_smbus_read_word_swapped(mpu_drvdata->mpu_i2c_client, chan->address);
		if (ret < 0) {
			return ret;
		}
		*val = (s16)ret;
		return IIO_VAL_INT;
	case IIO_CHAN_INFO_SCALE:
		switch (chan->type) {
		case IIO_TEMP:
			// scale = val/val2
			*val = 1000;
			*val2 = 340;
			return IIO_VAL_FRACTIONAL; // linux hiển thị dạng phân số
		case IIO_ACCEL:
			*val = 0;
			*val2 = 598;
			return IIO_VAL_INT_PLUS_MICRO; // linux hiển thị dạng
		default:
			break;
		}
		break;
	default:
		break;
	}

	return ret;
}

static const struct iio_info mpu_iio_info = {
    .read_raw = mpu_read_raw,
};

// Tổ hợp kênh mà driver cho phép đọc
static const unsigned long mpu_available_scan_masks[] = {
    // 0 1 2 = X Y Z theo scan_index trong iio_chan_spec
    BIT(0) | BIT(1) | BIT(2) | BIT(4),
    BIT(0) | BIT(1) | BIT(2),
    BIT(4), // temperature
    0,
};

static const struct iio_chan_spec mpu_iio_chan_spec[] = {
    /*
	    - nếu có các trục rõ ràng: modified = 1, channel2 = IIO_MOD_n (n = X,Y,Z
	    - nếu có các kênh không phân trục: modified = 0, indexed = 1, channel = 0 hoặc 1 cho từng kênh
		- nếu là kênh duy nhất không cùng loại với cái khác: modified = 0, indexed = 0, channel = 0
    */
    {
	.type = IIO_TEMP,
	.indexed = 0,
	.channel = 0,
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),
	.scan_index = 4,
	.scan_type = {
	    .sign = 's',	  // giá trị là kiểu signed
	    .realbits = 16,	  // mpu6050 có ADC-16bit
	    .storagebits = 16,	  // cần 16bit để chứa đủ cho realbits
	    .shift = 0,		  // dịch dữ liệu đi x bit so với đầu thanh ghi
	    .endianness = IIO_BE, // byte cao đứng trước
	},
	.output = 0,
	.address = 0x41, // địa chỉ của thanh ghi chứa kênh này
    },

    MPU6050_ACCEL_CHANNEL(0, 0x3B, X),
    MPU6050_ACCEL_CHANNEL(1, 0x3D, Y),
    MPU6050_ACCEL_CHANNEL(2, 0x3F, Z),

    // Khai báo kênh timestamp để lấy thời điểm xảy ra ngắt, thời gian này là thời gian trong linux
    IIO_CHAN_SOFT_TIMESTAMP(5), // 5 là scan index phải lớn hơn cái hiện tại
};

static const struct iio_trigger_ops mpu_iio_trigger_ops = {
    .set_trigger_state = mpu_set_trigger_state,
    .validate_device = NULL, // dùng khi muốn 1 trigger của device khác ăn theo trigger của mpu6050 khi mpu6050 được trigger
};
int mpu_set_trigger_state(struct iio_trigger *trig, bool state) {
	// hàm này có nhiệm vụ bật/tắt việc truyền ngắt trên mpu6050 theo state mà linux gửi
	// mục tiêu: + quản lý năng lượng: bật tắt khi cần tránh ngắt gửi quá nhiều không cần thiết
	//			 + đồng bộ hóa dữ liệu
	// pr_info("%s\n", __func__);
	u8 buf = state ? 0x11 : 0x00;
	struct mpu_drvdata *mpu_drvdata = iio_trigger_get_drvdata(trig);
	return i2c_smbus_write_byte_data(mpu_drvdata->mpu_i2c_client, 0x38, buf);
}

// const struct iio_buffer_setup_ops *mpu_iio_buffer_setup_ops = {
//     // todo
// };

static irqreturn_t mpu_trigger_handler(int irq, void *p) {
	// pr_info("%s\n", __func__);

	struct {
		u16 channels[4];
		s64 timestamp __aligned(8);
	} scan_data;

	// p là iio_poll_func được devm_iio_triggered_buffer_setup truyền vào
	struct iio_poll_func *pf = p;
	struct iio_dev *iio_dev = pf->indio_dev;
	struct mpu_drvdata *mpu_drvdata = iio_priv(iio_dev);

	// đọc data từ cảm biến (đọc khối 8 byte liên tục tương đương từ 0x3B tới 0x42)
	i2c_smbus_read_i2c_block_data(mpu_drvdata->mpu_i2c_client, 0x3B, 8, (u8 *)scan_data.channels);

	// đẩy data vào kernel buffer
	iio_push_to_buffers_with_timestamp(iio_dev, &scan_data, pf->timestamp);

	// báo hoàn thành cho IIO core, không báo thì IIO subsystem nghĩ là chưa xong
	iio_trigger_notify_done(iio_dev->trig);

	return IRQ_HANDLED;
}

static int mpu_probe(struct i2c_client *client) {
	pr_info("Detected %s\n", client->name);

	int ret;
	struct iio_dev *mpu_iio_dev;
	struct mpu_drvdata *mpu_drvdata;
	struct iio_trigger *mpu_iio_trigger;
	u8 buf;

	// ===================== quản lý device trong linux =========================
	mpu_iio_dev = devm_iio_device_alloc(&client->dev, sizeof(struct mpu_drvdata));
	if (!mpu_iio_dev) {
		pr_info("%d cannot allocate mpu_iio_dev\n", __LINE__);
		return -ENOMEM;
	}
	mpu_drvdata = iio_priv(mpu_iio_dev);
	mpu_drvdata->mpu_i2c_client = client;
	mpu_drvdata->mpu_iio_dev = mpu_iio_dev;

	i2c_set_clientdata(client, mpu_iio_dev);

	mpu_iio_dev->name = "mpu6050";
	mpu_iio_dev->info = &mpu_iio_info; // chứa hàm đọc, ghi như fops
	mpu_iio_dev->modes = INDIO_DIRECT_MODE;
	mpu_iio_dev->available_scan_masks = mpu_available_scan_masks; // tổ hợp các kênh mà driver cho phép bật để đọc qua buffer
	mpu_iio_dev->channels = mpu_iio_chan_spec;		      // cách đo của từng loại dữ liệu
	mpu_iio_dev->num_channels = ARRAY_SIZE(mpu_iio_chan_spec);    // khai báo số lượng kênh của mpu_iio_chan_spec

	// ===================== cấu hình phần cứng cho mpu6050 ===================
	buf = 0x80; // reset all internal registers
	i2c_smbus_write_byte_data(client, 0x6B, buf);
	msleep(200);

	buf = 0x7; // reset gyro, accel, temp registers
	i2c_smbus_write_byte_data(client, 0x68, buf);
	msleep(200);

	buf = 0x01; // wake up
	i2c_smbus_write_byte_data(client, 0x6B, buf);
	msleep(200);

	// kiểm tra định danh chip
	ret = i2c_smbus_read_byte_data(client, 0x75);
	if (ret != 0x68 && ret != 0xEB && ret != 0xE9) {
		pr_info("Not mpu6050 %d\n", ret);
		// return -ENODEV;
	}

	// cấu hình tần số lấy mẫu (bao lâu thì kích hoạt ngắt iio_trigger_generic_data_rdy_poll 1 lần) và bộ lọc thông thấp DLPF
	buf = 0x04;
	i2c_smbus_write_byte_data(client, 0x19, buf);
	buf = 0x01;
	i2c_smbus_write_byte_data(client, 0x1A, buf);

	// cấu hình dải đo
	buf = 0x00;
	i2c_smbus_write_byte_data(client, 0x1C, buf);

	// cấu hình chân ngắt
	buf = 0x30;
	i2c_smbus_write_byte_data(client, 0x37, buf);

	// ===================== quản lý trigger =========================
	// đăng ký trigger để kích hoạt việc lấy dữ liệu (từ userspace, sự kiện phần cứng, ...) để CPU không phải đọc cảm biến liên tục (Cơ chế này hiểu như ngắt)
	// tạo ra trong /sys/bus/iio/devices/triggerX/
	mpu_iio_trigger = devm_iio_trigger_alloc(&client->dev, "%s-dev%d", mpu_iio_dev->name, iio_device_id(mpu_iio_dev));
	if (!mpu_iio_trigger) {
		pr_info("%d cannot allocate mpu_iio_trigger\n", __LINE__);
		return -ENOMEM;
	}
	mpu_iio_trigger->ops = &mpu_iio_trigger_ops;
	iio_trigger_set_drvdata(mpu_iio_trigger, mpu_drvdata); // gán mpu_drvdata vào trong data của mpu_iio_trigger
	mpu_drvdata->mpu_iio_trigger = mpu_iio_trigger;

	// đăng ký ngắt để khi chân INT của mpu6050 lên high thì ngắt này được gọi
	ret = devm_request_threaded_irq(&client->dev, client->irq,
					&iio_trigger_generic_data_rdy_poll, // báo hiệu có ngắt từ chân ngắt của mpu6050; để làm top half vì hàm này chỉ báo có ngắt
					NULL,
					IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					"mpu6050_irq",	  // tên sẽ hiển thị trong /proc/interrupts
					mpu_iio_trigger); // khi ngắt xảy ra sẽ truyền mpu_iio_trigger vào iio_trigger_generic_data_rdy_poll

	// ===================== quản lý buffer =========================
	/*
		- buffer được lưu trong vùng đệm kernel
		- cảm biến có tần số lấy mẫu cao (từ vài chục Hz trở lên) sẽ gửi data rất nhiều
		- CPU nếu không dùng buffer thì phải liên tục đọc dataa từ cảm biến
		- việc dùng buffer giúp CPU sau 1 chu kỳ thì lấy buffer 1 lần, tránh phải đọc liên tục
	*/

	// ngắt này được kích hoạt bới việc gọi iio_trigger_generic_data_rdy_poll
	devm_iio_triggered_buffer_setup(&client->dev, mpu_iio_dev, NULL,
					mpu_trigger_handler, // xử lý buffer sau thời gian lấy mẫu
					NULL);		     // mpu_iio_buffer_setup_ops

	// tạo ra device trong /sys
	devm_iio_trigger_register(&client->dev, mpu_iio_trigger);
	// tạo ra device trong /dev và /sys
	ret = devm_iio_device_register(&client->dev, mpu_iio_dev);

	return 0;
}

static void mpu_remove(struct i2c_client *client) {
	pr_info("Remove mpu6050\n");
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Control MPU_6050");
MODULE_AUTHOR("Nguyen Dung");