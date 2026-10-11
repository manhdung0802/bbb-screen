#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/module.h>

static int mpu_probe(struct i2c_client *client);
static void mpu_remove(struct i2c_client *client);

static const struct of_device_id mpu_dt_match[] = {
    {.compatible = "mpu6050"},
    {},
};
MODULE_DEVICE_TABLE(of, mpu_dt_match);

static struct i2c_driver mpu_driver = {
    .probe = mpu_probe,
    .remove = mpu_remove,
    .driver = {
	.name = "mpu6050",
	.of_match_table = mpu_dt_match,
    },
};
module_i2c_driver(mpu_driver);

struct mpu_dev {
	struct i2c_client *i2c_client;
};

static int mpu_probe(struct i2c_client *i2c_client) {
	pr_info("Detect mpu6050\n");
	struct mpu_dev *mpu_dev;
	u8 buf_send[2];
	int ret;

	mpu_dev = devm_kzalloc(&i2c_client->dev, sizeof(*mpu_dev), GFP_KERNEL);
	if (!mpu_dev) {
		return -ENOMEM;
	}
	mpu_dev->i2c_client = i2c_client;

	return 0;
}

static void mpu_remove(struct i2c_client *client) {
	pr_info("Remove mpu6050\n");
}

MODULE_LICENSE("GPL");