#include <fcntl.h>
#include <iio.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

static struct iio_context *ctx = NULL;
static struct iio_buffer *rxbuf = NULL;
static volatile bool running = true;

static void sigint_handler(int signum) {
	(void)signum;
	running = false;
}

int main(void) {
	struct sigaction act;
	act.sa_handler = sigint_handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;

	if (sigaction(SIGINT, &act, NULL) == -1) {
		perror("Lỗi đăng ký tín hiệu SIGINT");
		return 1;
	}

	// 1. Tạo local context của libiio
	ctx = iio_create_local_context();
	if (!ctx) {
		fprintf(stderr, "Không thể tạo iio local context\n");
		return -1;
	}

	// 2. Tìm thiết bị mpu6050
	struct iio_device *dev = iio_context_find_device(ctx, "mpu6050");
	if (!dev) {
		fprintf(stderr, "Không tìm thấy thiết bị mpu6050\n");
		iio_context_destroy(ctx);
		return -1;
	}

	// 3. Tìm và gán Trigger (tên trigger trong driver là "mpu6050-dev0")
	const struct iio_device *trig = iio_context_find_device(ctx, "mpu6050-dev0");
	if (!trig) {
		// Tìm kiếm fallback nếu tên có chỉ số khác
		for (unsigned int i = 0; i < iio_context_get_devices_count(ctx); i++) {
			const struct iio_device *d = iio_context_get_device(ctx, i);
			const char *name = iio_device_get_name(d);
			if (iio_device_is_trigger(d) && name && strstr(name, "mpu6050")) {
				trig = d;
				break;
			}
		}
	}

	if (trig) {
		if (iio_device_set_trigger(dev, trig) < 0) {
			perror("Lỗi iio_device_set_trigger");
		} else {
			printf("Đã gán trigger: %s\n", iio_device_get_name(trig));
		}
	} else {
		fprintf(stderr, "Cảnh báo: Không tìm thấy trigger mpu6050!\n");
	}

	// 4. Kích hoạt các kênh dữ liệu
	// Lưu ý: Driver mpu6050.c yêu cầu tổ hợp kênh (available_scan_masks):
	// Hoặc cả 3 trục Accel (X, Y, Z), hoặc Accel + Temp, hoặc chỉ Temp.
	struct iio_channel *chn_x = iio_device_find_channel(dev, "accel_x", false);
	struct iio_channel *chn_y = iio_device_find_channel(dev, "accel_y", false);
	struct iio_channel *chn_z = iio_device_find_channel(dev, "accel_z", false);
	struct iio_channel *chn_temp = iio_device_find_channel(dev, "temp", false);

	if (!chn_x || !chn_y || !chn_z) {
		fprintf(stderr, "Không tìm thấy các kênh accel\n");
		iio_context_destroy(ctx);
		return -1;
	}

	iio_channel_enable(chn_x);
	iio_channel_enable(chn_y);
	iio_channel_enable(chn_z);
	if (chn_temp) {
		iio_channel_enable(chn_temp);
	}

	// 5. Tạo buffer (Hàm này tự động kích hoạt buffer/enable = 1 trong kernel)
	rxbuf = iio_device_create_buffer(dev, 10, false);
	if (!rxbuf) {
		perror("Lỗi iio_device_create_buffer");
		iio_context_destroy(ctx);
		return -1;
	}

	// 6. Lấy file descriptor từ buffer để đăng ký vào epoll
	int fd = iio_buffer_get_poll_fd(rxbuf);
	if (fd < 0) {
		perror("Lỗi iio_buffer_get_poll_fd");
		iio_buffer_destroy(rxbuf);
		iio_context_destroy(ctx);
		return -1;
	}

	int epoll_fd = epoll_create1(0);
	if (epoll_fd < 0) {
		perror("Lỗi epoll_create1");
		iio_buffer_destroy(rxbuf);
		iio_context_destroy(ctx);
		return -1;
	}

	struct epoll_event event = {
	    .events = EPOLLIN,
	    .data.fd = fd,
	};

	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) < 0) {
		perror("Lỗi epoll_ctl");
		close(epoll_fd);
		iio_buffer_destroy(rxbuf);
		iio_context_destroy(ctx);
		return -1;
	}

	printf("Khởi tạo thành công! Đang chờ ngắt MPU6050 qua epoll...\n");

	// 7. Vòng lặp chờ ngắt và đọc dữ liệu
	while (running) {
		int ret = epoll_wait(epoll_fd, &event, 1, 1000);
		if (ret < 0) {
			// Thoát khi nhận tín hiệu SIGINT (Ctrl+C)
			break;
		} else if (ret == 0) {
			// Hết thời gian chờ (timeout 1000ms), tiếp tục vòng lặp
			continue;
		}

		if (event.events & EPOLLIN) {
			// Lấy dữ liệu từ kernel buffer vào userspace
			ssize_t bytes = iio_buffer_refill(rxbuf);
			if (bytes > 0) {
				int16_t x = 0, y = 0, z = 0, raw_temp = 0;
				iio_channel_read(chn_x, rxbuf, &x, sizeof(x));
				iio_channel_read(chn_y, rxbuf, &y, sizeof(y));
				iio_channel_read(chn_z, rxbuf, &z, sizeof(z));
				if (chn_temp) {
					iio_channel_read(chn_temp, rxbuf, &raw_temp, sizeof(raw_temp));
				}

				float temp_c = ((float)raw_temp / 340.0f) + 36.53f;
				printf("[New event] Accel: X=%6d | Y=%6d | Z=%6d | Temp=%.2f°C\n",
				       x, y, z, temp_c);
			} else {
				perror("Lỗi iio_buffer_refill");
			}
		}
	}

	printf("\nĐang dọn dẹp và thoát...\n");
	close(epoll_fd);
	iio_buffer_destroy(rxbuf); // Tự động ghi 0 vào buffer/enable
	if (dev) {
		iio_device_set_trigger(dev, NULL); // Nhả trigger để đưa refcnt về 0
	}
	iio_context_destroy(ctx);

	return 0;
}