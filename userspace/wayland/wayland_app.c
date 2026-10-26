#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <iio.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"

// Kích thước cửa sổ lớn, tối ưu cho màn hình 1024x768 của BBB
#define WIN_WIDTH     980
#define WIN_HEIGHT    680
#define GRAPH_SAMPLES 320

// ==================== CẤU TRÚC DỮ LIỆU SENSOR MPU6050 ====================
struct sensor_data {
	int16_t accel_x;
	int16_t accel_y;
	int16_t accel_z;
	float temp_c;
	bool connected;
	uint32_t sample_count;
};

static struct sensor_data g_sensor = {0};
static pthread_mutex_t g_sensor_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t g_mpu_thread;

// ==================== BẢNG FONT BITMAP 8x8 (ASCII 32 - 127) ====================
// Mỗi ký tự gồm 8 bytes (MSB là pixel bên trái nhất, LSB là bên phải)
static const uint8_t font8x8_basic[96][8] = {
	{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, //   32
	{0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, // ! 33
	{0x66,0x66,0x24,0x00,0x00,0x00,0x00,0x00}, // " 34
	{0x6C,0x6C,0xFE,0x6C,0xFE,0x6C,0x6C,0x00}, // # 35
	{0x18,0x3E,0x60,0x3C,0x06,0x7C,0x18,0x00}, // $ 36
	{0x00,0x63,0x66,0x0C,0x18,0x33,0x63,0x00}, // % 37
	{0x38,0x6C,0x38,0x76,0xDC,0xCC,0x76,0x00}, // & 38
	{0x18,0x18,0x30,0x00,0x00,0x00,0x00,0x00}, // ' 39
	{0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0x00}, // ( 40
	{0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0x00}, // ) 41
	{0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, // * 42
	{0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00}, // + 43
	{0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30}, // , 44
	{0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}, // - 45
	{0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, // . 46
	{0x06,0x0C,0x18,0x30,0x60,0xC0,0x80,0x00}, // / 47
	{0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0x00}, // 0 48
	{0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00}, // 1 49
	{0x3C,0x66,0x06,0x1C,0x30,0x66,0x7E,0x00}, // 2 50
	{0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00}, // 3 51
	{0x0C,0x1C,0x3C,0x6C,0xFE,0x0C,0x0C,0x00}, // 4 52
	{0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00}, // 5 53
	{0x1C,0x30,0x60,0x7C,0x66,0x66,0x3C,0x00}, // 6 54
	{0x7E,0x06,0x0C,0x18,0x30,0x30,0x30,0x00}, // 7 55
	{0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00}, // 8 56
	{0x3C,0x66,0x66,0x3E,0x06,0x0C,0x38,0x00}, // 9 57
	{0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00}, // : 58
	{0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x30}, // ; 59
	{0x0C,0x18,0x30,0x60,0x30,0x18,0x0C,0x00}, // < 60
	{0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00}, // = 61
	{0x30,0x18,0x0C,0x06,0x0C,0x18,0x30,0x00}, // > 62
	{0x3C,0x66,0x0C,0x18,0x18,0x00,0x18,0x00}, // ? 63
	{0x3C,0x66,0x6E,0x6E,0x60,0x62,0x3C,0x00}, // @ 64
	{0x3C,0x66,0x66,0x7E,0x66,0x66,0x66,0x00}, // A 65
	{0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0x00}, // B 66
	{0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00}, // C 67
	{0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0x00}, // D 68
	{0x7E,0x60,0x60,0x7C,0x60,0x60,0x7E,0x00}, // E 69
	{0x7E,0x60,0x60,0x7C,0x60,0x60,0x60,0x00}, // F 70
	{0x3C,0x66,0x60,0x6E,0x66,0x66,0x3C,0x00}, // G 71
	{0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00}, // H 72
	{0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // I 73
	{0x0E,0x06,0x06,0x06,0x66,0x66,0x3C,0x00}, // J 74
	{0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0x00}, // K 75
	{0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00}, // L 76
	{0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00}, // M 77
	{0x66,0x76,0x7E,0x7E,0x6E,0x66,0x66,0x00}, // N 78
	{0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, // O 79
	{0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00}, // P 80
	{0x3C,0x66,0x66,0x66,0x6A,0x6C,0x36,0x00}, // Q 81
	{0x7C,0x66,0x66,0x7C,0x6C,0x66,0x66,0x00}, // R 82
	{0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00}, // S 83
	{0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, // T 84
	{0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, // U 85
	{0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00}, // V 86
	{0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, // W 87
	{0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00}, // X 88
	{0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00}, // Y 89
	{0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00}, // Z 90
	{0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00}, // [ 91
	{0xC0,0x60,0x30,0x18,0x0C,0x06,0x02,0x00}, // \ 92
	{0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00}, // ] 93
	{0x18,0x3C,0x66,0x00,0x00,0x00,0x00,0x00}, // ^ 94
	{0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x00}, // _ 95
	{0x30,0x18,0x0C,0x00,0x00,0x00,0x00,0x00}, // ` 96
	{0x00,0x00,0x3C,0x06,0x3E,0x66,0x3E,0x00}, // a 97
	{0x60,0x60,0x7C,0x66,0x66,0x66,0x7C,0x00}, // b 98
	{0x00,0x00,0x3C,0x66,0x60,0x66,0x3C,0x00}, // c 99
	{0x06,0x06,0x3E,0x66,0x66,0x66,0x3E,0x00}, // d 100
	{0x00,0x00,0x3C,0x66,0x7E,0x60,0x3C,0x00}, // e 101
	{0x0E,0x18,0x7E,0x18,0x18,0x18,0x18,0x00}, // f 102
	{0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x3C}, // g 103
	{0x60,0x60,0x7C,0x66,0x66,0x66,0x66,0x00}, // h 104
	{0x18,0x00,0x38,0x18,0x18,0x18,0x3C,0x00}, // i 105
	{0x06,0x00,0x0E,0x06,0x06,0x66,0x66,0x3C}, // j 106
	{0x60,0x60,0x66,0x6C,0x78,0x6C,0x66,0x00}, // k 107
	{0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // l 108
	{0x00,0x00,0x66,0x7F,0x7F,0x6B,0x63,0x00}, // m 109
	{0x00,0x00,0x7C,0x66,0x66,0x66,0x66,0x00}, // n 110
	{0x00,0x00,0x3C,0x66,0x66,0x66,0x3C,0x00}, // o 111
	{0x00,0x00,0x7C,0x66,0x66,0x7C,0x60,0x60}, // p 112
	{0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x06}, // q 113
	{0x00,0x00,0x7C,0x66,0x60,0x60,0x60,0x00}, // r 114
	{0x00,0x00,0x3E,0x60,0x3C,0x06,0x7C,0x00}, // s 115
	{0x18,0x18,0x7E,0x18,0x18,0x18,0x0E,0x00}, // t 116
	{0x00,0x00,0x66,0x66,0x66,0x66,0x3E,0x00}, // u 117
	{0x00,0x00,0x66,0x66,0x66,0x3C,0x18,0x00}, // v 118
	{0x00,0x00,0x63,0x6B,0x7F,0x3E,0x36,0x00}, // w 119
	{0x00,0x00,0x66,0x3C,0x18,0x3C,0x66,0x00}, // x 120
	{0x00,0x00,0x66,0x66,0x66,0x3E,0x06,0x3C}, // y 121
	{0x00,0x00,0x7E,0x0C,0x18,0x30,0x7E,0x00}, // z 122
	{0x0E,0x18,0x18,0x70,0x18,0x18,0x0E,0x00}, // { 123
	{0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, // | 124
	{0x70,0x18,0x18,0x0E,0x18,0x18,0x70,0x00}, // } 125
	{0x3B,0x6E,0x00,0x00,0x00,0x00,0x00,0x00}, // ~ 126
	{0x1C,0x36,0x36,0x1C,0x00,0x00,0x00,0x00}, // Degree symbol ° (index 95)
};

// ==================== CẤU TRÚC ỨNG DỤNG WAYLAND ====================
struct app_state {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct wl_shm *shm;
	struct xdg_wm_base *wm_base;

	struct wl_surface *surface;
	struct xdg_surface *xdg_surface;
	struct xdg_toplevel *xdg_toplevel;

	struct wl_buffer *buffer;
	uint32_t *pixel_data;
	int width;
	int height;

	bool configured;
	volatile bool running;

	// Bộ nhớ đệm đồ thị sóng
	int16_t wave_x[GRAPH_SAMPLES];
	int16_t wave_y[GRAPH_SAMPLES];
	int16_t wave_z[GRAPH_SAMPLES];
	int wave_idx;

	// Animation góc nghiêng (Bubble Level)
	float smooth_ball_x;
	float smooth_ball_y;
};

static struct app_state app = {
	.width = WIN_WIDTH,
	.height = WIN_HEIGHT,
	.running = true,
	.configured = false,
	.wave_idx = 0,
	.smooth_ball_x = 0.0f,
	.smooth_ball_y = 0.0f,
};

static void sigint_handler(int signum) {
	(void)signum;
	app.running = false;
}

// ==================== HÀM VẼ ĐỒ HỌA ====================
static inline void set_pixel(uint32_t *p, int stride, int x, int y, uint32_t color) {
	if (x >= 0 && x < app.width && y >= 0 && y < app.height) {
		p[y * stride + x] = color;
	}
}

static void draw_rect(uint32_t *p, int stride, int rx, int ry, int rw, int rh, uint32_t color) {
	for (int y = ry; y < ry + rh && y < app.height; y++) {
		if (y < 0) continue;
		for (int x = rx; x < rx + rw && x < app.width; x++) {
			if (x < 0) continue;
			p[y * stride + x] = color;
		}
	}
}

static void draw_circle(uint32_t *pixels, int stride, int cx, int cy, int radius, uint32_t color) {
	int r2 = radius * radius;
	for (int y = cy - radius; y <= cy + radius; y++) {
		if (y < 0 || y >= app.height) continue;
		for (int x = cx - radius; x <= cx + radius; x++) {
			if (x < 0 || x >= app.width) continue;
			int dx = x - cx;
			int dy = y - cy;
			if (dx * dx + dy * dy <= r2) {
				pixels[y * stride + x] = color;
			}
		}
	}
}

static void draw_circle_outline(uint32_t *pixels, int stride, int cx, int cy, int radius, uint32_t color) {
	int r_inner2 = (radius - 1) * (radius - 1);
	int r_outer2 = (radius + 1) * (radius + 1);
	for (int y = cy - radius - 1; y <= cy + radius + 1; y++) {
		if (y < 0 || y >= app.height) continue;
		for (int x = cx - radius - 1; x <= cx + radius + 1; x++) {
			if (x < 0 || x >= app.width) continue;
			int dx = x - cx;
			int dy = y - cy;
			int dist2 = dx * dx + dy * dy;
			if (dist2 >= r_inner2 && dist2 <= r_outer2) {
				pixels[y * stride + x] = color;
			}
		}
	}
}

// Vẽ ký tự 8x8 đúng chiều chuẩn (MSB -> LSB)
static void draw_char(uint32_t *pixels, int stride, int x, int y, char c, uint32_t fg, int scale) {
	int idx = (unsigned char)c - 32;
	if (c == 0x7F || (unsigned char)c == 0xB0 || (unsigned char)c == 0xDF) idx = 95; // Ký hiệu độ °
	if (idx < 0 || idx > 95) idx = 0;

	for (int row = 0; row < 8; row++) {
		uint8_t bits = font8x8_basic[idx][row];
		for (int col = 0; col < 8; col++) {
			// SỬA: Dùng 0x80 >> col thay vì 1 << col để không bị ngược chữ
			if (bits & (0x80 >> col)) {
				for (int sy = 0; sy < scale; sy++) {
					for (int sx = 0; sx < scale; sx++) {
						set_pixel(pixels, stride, x + col * scale + sx, y + row * scale + sy, fg);
					}
				}
			}
		}
	}
}

static void draw_string(uint32_t *pixels, int stride, int x, int y, const char *str, uint32_t fg, int scale) {
	int cur_x = x;
	while (*str) {
		draw_char(pixels, stride, cur_x, y, *str, fg, scale);
		cur_x += 8 * scale;
		str++;
	}
}

// ==================== LUỒNG ĐỌC MPU6050 QUA IIO BUFFER ====================
static void *mpu6050_worker_thread(void *arg) {
	(void)arg;
	printf("[MPU Thread] Bắt đầu khởi tạo IIO...\n");

	while (app.running) {
		struct iio_context *ctx = iio_create_local_context();
		if (!ctx) {
			sleep(1);
			continue;
		}

		struct iio_device *dev = iio_context_find_device(ctx, "mpu6050");
		if (!dev) {
			printf("[MPU Thread] Đang tìm thiết bị mpu6050...\n");
			iio_context_destroy(ctx);
			sleep(1);
			continue;
		}

		const struct iio_device *trig = iio_context_find_device(ctx, "mpu6050-dev0");
		if (!trig) {
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
				perror("[MPU Thread] Lỗi gán trigger");
			} else {
				printf("[MPU Thread] Đã gán trigger: %s\n", iio_device_get_name(trig));
			}
		} else {
			printf("[MPU Thread] Cảnh báo: Không tìm thấy trigger mpu6050!\n");
		}

		struct iio_channel *chn_x = iio_device_find_channel(dev, "accel_x", false);
		struct iio_channel *chn_y = iio_device_find_channel(dev, "accel_y", false);
		struct iio_channel *chn_z = iio_device_find_channel(dev, "accel_z", false);
		struct iio_channel *chn_temp = iio_device_find_channel(dev, "temp", false);

		if (!chn_x || !chn_y || !chn_z) {
			printf("[MPU Thread] Không tìm thấy kênh accel!\n");
			iio_context_destroy(ctx);
			sleep(1);
			continue;
		}

		iio_channel_enable(chn_x);
		iio_channel_enable(chn_y);
		iio_channel_enable(chn_z);
		if (chn_temp) iio_channel_enable(chn_temp);

		struct iio_buffer *rxbuf = iio_device_create_buffer(dev, 10, false);
		if (!rxbuf) {
			perror("[MPU Thread] Lỗi iio_device_create_buffer");
			iio_context_destroy(ctx);
			sleep(1);
			continue;
		}

		int fd = iio_buffer_get_poll_fd(rxbuf);
		if (fd < 0) {
			perror("[MPU Thread] Lỗi iio_buffer_get_poll_fd");
			iio_buffer_destroy(rxbuf);
			iio_context_destroy(ctx);
			sleep(1);
			continue;
		}

		int epoll_fd = epoll_create1(0);
		struct epoll_event ev = { .events = EPOLLIN, .data.fd = fd };
		epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);

		printf("[MPU Thread] Đã kết nối MPU6050 thành công!\n");

		while (app.running) {
			struct epoll_event event;
			int ret = epoll_wait(epoll_fd, &event, 1, 500);
			if (ret <= 0) continue;

			if (event.events & EPOLLIN) {
				ssize_t bytes = iio_buffer_refill(rxbuf);
				if (bytes > 0) {
					int16_t x = 0, y = 0, z = 0, raw_t = 0;
					iio_channel_read(chn_x, rxbuf, &x, sizeof(x));
					iio_channel_read(chn_y, rxbuf, &y, sizeof(y));
					iio_channel_read(chn_z, rxbuf, &z, sizeof(z));
					if (chn_temp) {
						iio_channel_read(chn_temp, rxbuf, &raw_t, sizeof(raw_t));
					}

					float temp_c = ((float)raw_t / 340.0f) + 36.53f;

					pthread_mutex_lock(&g_sensor_mutex);
					g_sensor.accel_x = x;
					g_sensor.accel_y = y;
					g_sensor.accel_z = z;
					g_sensor.temp_c = temp_c;
					g_sensor.connected = true;
					g_sensor.sample_count++;
					pthread_mutex_unlock(&g_sensor_mutex);
				}
			}
		}

		close(epoll_fd);
		iio_buffer_destroy(rxbuf);
		if (dev) {
			iio_device_set_trigger(dev, NULL); // Nhả trigger để đưa refcnt về 0
		}
		iio_context_destroy(ctx);
		break;
	}

	printf("[MPU Thread] Đã dừng luồng sensor.\n");
	return NULL;
}

// ==================== VẼ TOÀN BỘ GIAO DIỆN DASHBOARD (980x680) ====================
static void render_frame(void) {
	int stride = app.width;
	uint32_t *p = app.pixel_data;

	// Lấy dữ liệu sensor
	struct sensor_data s;
	pthread_mutex_lock(&g_sensor_mutex);
	s = g_sensor;
	pthread_mutex_unlock(&g_sensor_mutex);

	// Lưu vào lịch sử waveform
	app.wave_x[app.wave_idx] = s.accel_x;
	app.wave_y[app.wave_idx] = s.accel_y;
	app.wave_z[app.wave_idx] = s.accel_z;
	app.wave_idx = (app.wave_idx + 1) % GRAPH_SAMPLES;

	// 1. Nền tổng thể: Dark Theme (#181825)
	for (int i = 0; i < app.width * app.height; i++) {
		p[i] = 0xFF181825;
	}

	// 2. Thanh tiêu đề (Header): Cao 50px
	for (int y = 0; y < 50; y++) {
		float f = (float)y / 50.0f;
		uint8_t r = (uint8_t)(20 + f * 30);
		uint8_t g = (uint8_t)(60 + f * 50);
		uint8_t b = (uint8_t)(160 + f * 70);
		uint32_t color = 0xFF000000 | (r << 16) | (g << 8) | b;
		for (int x = 0; x < app.width; x++) {
			p[y * stride + x] = color;
		}
	}

	draw_string(p, stride, 20, 16, "BEAGLEBONE BLACK", 0xFF89B4FA, 2);
	draw_string(p, stride, 330, 16, "MPU-6050 REAL-TIME DASHBOARD", 0xFFCDD6F4, 2);

	// Đèn chỉ báo trạng thái (Status LED)
	uint32_t led_col = s.connected ? 0xFFA6E3A1 : 0xFFF38BA8;
	draw_circle(p, stride, app.width - 30, 25, 9, led_col);

	// ==================== CARD 1: 3-AXIS ACCELEROMETER ====================
	// Vị trí: x=20, y=65, w=450, h=290
	draw_rect(p, stride, 20, 65, 450, 290, 0xFF1E1E2E);
	draw_rect(p, stride, 20, 65, 5, 290, 0xFF89B4FA); // Viền lam
	draw_string(p, stride, 35, 78, "ACCELEROMETER (16-BIT RAW)", 0xFF89B4FA, 2);

	char buf[64];
	const int bar_max_w = 200;
	const int bar_cx = 325;

	// Trục X (Đỏ)
	snprintf(buf, sizeof(buf), "X: %+6d", s.accel_x);
	draw_string(p, stride, 35, 125, buf, 0xFFF38BA8, 2);
	draw_rect(p, stride, bar_cx - bar_max_w / 2, 125, bar_max_w, 16, 0xFF313244);
	int w_x = (int)((float)s.accel_x / 16384.0f * (bar_max_w / 2.0f));
	if (w_x > bar_max_w / 2) w_x = bar_max_w / 2;
	if (w_x < -bar_max_w / 2) w_x = -bar_max_w / 2;
	if (w_x >= 0) draw_rect(p, stride, bar_cx, 125, w_x, 16, 0xFFF38BA8);
	else draw_rect(p, stride, bar_cx + w_x, 125, -w_x, 16, 0xFFF38BA8);

	// Trục Y (Xanh lục)
	snprintf(buf, sizeof(buf), "Y: %+6d", s.accel_y);
	draw_string(p, stride, 35, 175, buf, 0xFFA6E3A1, 2);
	draw_rect(p, stride, bar_cx - bar_max_w / 2, 175, bar_max_w, 16, 0xFF313244);
	int w_y = (int)((float)s.accel_y / 16384.0f * (bar_max_w / 2.0f));
	if (w_y > bar_max_w / 2) w_y = bar_max_w / 2;
	if (w_y < -bar_max_w / 2) w_y = -bar_max_w / 2;
	if (w_y >= 0) draw_rect(p, stride, bar_cx, 175, w_y, 16, 0xFFA6E3A1);
	else draw_rect(p, stride, bar_cx + w_y, 175, -w_y, 16, 0xFFA6E3A1);

	// Trục Z (Xanh lam)
	snprintf(buf, sizeof(buf), "Z: %+6d", s.accel_z);
	draw_string(p, stride, 35, 225, buf, 0xFF89DCEB, 2);
	draw_rect(p, stride, bar_cx - bar_max_w / 2, 225, bar_max_w, 16, 0xFF313244);
	int w_z = (int)((float)s.accel_z / 16384.0f * (bar_max_w / 2.0f));
	if (w_z > bar_max_w / 2) w_z = bar_max_w / 2;
	if (w_z < -bar_max_w / 2) w_z = -bar_max_w / 2;
	if (w_z >= 0) draw_rect(p, stride, bar_cx, 225, w_z, 16, 0xFF89DCEB);
	else draw_rect(p, stride, bar_cx + w_z, 225, -w_z, 16, 0xFF89DCEB);

	// Tổng số sample
	snprintf(buf, sizeof(buf), "SAMPLES RECEIVED: %u", s.sample_count);
	draw_string(p, stride, 35, 310, buf, 0xFF6C7086, 2);

	// ==================== CARD 2: TEMPERATURE ====================
	// Vị trí: x=490, y=65, w=220, h=290
	draw_rect(p, stride, 490, 65, 220, 290, 0xFF1E1E2E);
	draw_rect(p, stride, 490, 65, 5, 290, 0xFFFAB387); // Viền cam
	draw_string(p, stride, 505, 78, "TEMPERATURE", 0xFFFAB387, 2);

	snprintf(buf, sizeof(buf), "%2.1f", s.temp_c);
	draw_string(p, stride, 515, 135, buf, 0xFFF9E2AF, 3);
	draw_char(p, stride, 615, 135, (char)127, 0xFFF9E2AF, 2); // Ký hiệu °
	draw_string(p, stride, 635, 135, "C", 0xFFF9E2AF, 3);

	// Thước đo nhiệt độ 15°C - 50°C
	draw_rect(p, stride, 510, 215, 180, 20, 0xFF313244);
	float temp_norm = (s.temp_c - 15.0f) / 35.0f;
	if (temp_norm < 0.0f) temp_norm = 0.0f;
	if (temp_norm > 1.0f) temp_norm = 1.0f;
	draw_rect(p, stride, 510, 215, (int)(temp_norm * 180.0f), 20, 0xFFFAB387);
	draw_string(p, stride, 510, 245, "15C            50C", 0xFF6C7086, 2);

	// ==================== CARD 3: 2D BUBBLE LEVEL (TILT INDICATOR) ====================
	// Vị trí: x=730, y=65, w=230, h=290
	draw_rect(p, stride, 730, 65, 230, 290, 0xFF1E1E2E);
	draw_rect(p, stride, 730, 65, 5, 290, 0xFFCBA6F7); // Viền tím
	draw_string(p, stride, 745, 78, "TILT LEVEL", 0xFFCBA6F7, 2);

	int center_x = 845;
	int center_y = 195;
	int target_radius = 65;

	// Các vòng tròn mục tiêu
	draw_circle_outline(p, stride, center_x, center_y, target_radius, 0xFF45475A);
	draw_circle_outline(p, stride, center_x, center_y, target_radius * 2 / 3, 0xFF313244);
	draw_circle_outline(p, stride, center_x, center_y, target_radius / 3, 0xFF313244);
	// Trục chữ thập
	draw_rect(p, stride, center_x - target_radius - 8, center_y, (target_radius + 8) * 2, 2, 0xFF313244);
	draw_rect(p, stride, center_x, center_y - target_radius - 8, 2, (target_radius + 8) * 2, 0xFF313244);

	// Quả bóng nghiêng phản hồi góc nghiêng
	float target_bx = -((float)s.accel_x / 16384.0f) * (target_radius - 12);
	float target_by =  ((float)s.accel_y / 16384.0f) * (target_radius - 12);

	app.smooth_ball_x += (target_bx - app.smooth_ball_x) * 0.25f;
	app.smooth_ball_y += (target_by - app.smooth_ball_y) * 0.25f;

	int bx = center_x + (int)app.smooth_ball_x;
	int by = center_y + (int)app.smooth_ball_y;
	draw_circle(p, stride, bx, by, 12, 0xFFF5C2E7);
	draw_circle(p, stride, bx, by, 6, 0xFFCBA6F7);

	draw_string(p, stride, 745, 310, "INCLINOMETER 2D", 0xFF6C7086, 2);

	// ==================== CARD 4: REAL-TIME OSCILLOSCOPE GRAPH ====================
	// Vị trí: x=20, y=370, w=940, h=290
	draw_rect(p, stride, 20, 370, 940, 290, 0xFF1E1E2E);
	draw_rect(p, stride, 20, 370, 5, 290, 0xFFA6E3A1); // Viền xanh lá
	draw_string(p, stride, 35, 385, "REAL-TIME WAVEFORM HISTORY", 0xFFA6E3A1, 2);

	// Chú thích màu
	draw_rect(p, stride, 600, 388, 16, 6, 0xFFF38BA8);
	draw_string(p, stride, 622, 385, "ACCEL X", 0xFFF38BA8, 2);

	draw_rect(p, stride, 730, 388, 16, 6, 0xFFA6E3A1);
	draw_string(p, stride, 752, 385, "ACCEL Y", 0xFFA6E3A1, 2);

	draw_rect(p, stride, 860, 388, 16, 6, 0xFF89DCEB);
	draw_string(p, stride, 882, 385, "ACCEL Z", 0xFF89DCEB, 2);

	// Khu vực đồ thị
	int graph_x = 35;
	int graph_y = 420;
	int graph_w = 910;
	int graph_h = 225;
	int zero_y = graph_y + graph_h / 2;

	// Lưới đồ thị: Đường trục 0
	draw_rect(p, stride, graph_x, zero_y, graph_w, 2, 0xFF45475A);
	// Các vạch phụ
	draw_rect(p, stride, graph_x, zero_y - 50, graph_w, 1, 0xFF2A2B3D);
	draw_rect(p, stride, graph_x, zero_y + 50, graph_w, 1, 0xFF2A2B3D);

	// Vẽ các đường đồ thị sóng
	for (int i = 0; i < GRAPH_SAMPLES - 1; i++) {
		int idx1 = (app.wave_idx + i) % GRAPH_SAMPLES;
		int px1 = graph_x + (i * graph_w) / GRAPH_SAMPLES;
		int px2 = graph_x + ((i + 1) * graph_w) / GRAPH_SAMPLES;

		// Trục X (Đỏ)
		int py1_x = zero_y - (int)((float)app.wave_x[idx1] / 32768.0f * (graph_h / 2.0f - 6.0f));
		for (int x = px1; x <= px2; x++) {
			set_pixel(p, stride, x, py1_x, 0xFFF38BA8);
			set_pixel(p, stride, x, py1_x + 1, 0xFFF38BA8);
		}

		// Trục Y (Xanh lá)
		int py1_y = zero_y - (int)((float)app.wave_y[idx1] / 32768.0f * (graph_h / 2.0f - 6.0f));
		for (int x = px1; x <= px2; x++) {
			set_pixel(p, stride, x, py1_y, 0xFFA6E3A1);
			set_pixel(p, stride, x, py1_y + 1, 0xFFA6E3A1);
		}

		// Trục Z (Xanh biển)
		int py1_z = zero_y - (int)((float)app.wave_z[idx1] / 32768.0f * (graph_h / 2.0f - 6.0f));
		for (int x = px1; x <= px2; x++) {
			set_pixel(p, stride, x, py1_z, 0xFF89DCEB);
			set_pixel(p, stride, x, py1_z + 1, 0xFF89DCEB);
		}
	}
}

// ==================== WAYLAND PROTOCOL HANDLERS ====================
static const struct wl_callback_listener frame_listener;

static void frame_done(void *data, struct wl_callback *callback, uint32_t time) {
	(void)data;
	(void)time;
	wl_callback_destroy(callback);

	if (!app.running) return;

	render_frame();

	wl_surface_attach(app.surface, app.buffer, 0, 0);
	wl_surface_damage_buffer(app.surface, 0, 0, app.width, app.height);

	struct wl_callback *cb = wl_surface_frame(app.surface);
	wl_callback_add_listener(cb, &frame_listener, NULL);

	wl_surface_commit(app.surface);
}

static const struct wl_callback_listener frame_listener = {
	.done = frame_done,
};

static void xdg_surface_handle_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
	(void)data;
	xdg_surface_ack_configure(xdg_surface, serial);

	if (!app.configured) {
		app.configured = true;

		render_frame();
		wl_surface_attach(app.surface, app.buffer, 0, 0);
		wl_surface_damage_buffer(app.surface, 0, 0, app.width, app.height);

		struct wl_callback *cb = wl_surface_frame(app.surface);
		wl_callback_add_listener(cb, &frame_listener, NULL);

		wl_surface_commit(app.surface);
	}
}

static const struct xdg_surface_listener xdg_surface_listener = {
	.configure = xdg_surface_handle_configure,
};

static void xdg_toplevel_handle_configure(void *data, struct xdg_toplevel *xdg_toplevel,
					 int32_t width, int32_t height, struct wl_array *states) {
	(void)data;
	(void)xdg_toplevel;
	(void)width;
	(void)height;
	(void)states;
}

static void xdg_toplevel_handle_close(void *data, struct xdg_toplevel *xdg_toplevel) {
	(void)data;
	(void)xdg_toplevel;
	app.running = false;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
	.configure = xdg_toplevel_handle_configure,
	.close = xdg_toplevel_handle_close,
};

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *wm_base, uint32_t serial) {
	(void)data;
	xdg_wm_base_pong(wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
	.ping = xdg_wm_base_ping,
};

static void registry_handle_global(void *data, struct wl_registry *registry,
				   uint32_t name, const char *interface, uint32_t version) {
	(void)data;
	(void)version;
	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		app.compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4);
	} else if (strcmp(interface, wl_shm_interface.name) == 0) {
		app.shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
	} else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
		app.wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
		xdg_wm_base_add_listener(app.wm_base, &xdg_wm_base_listener, NULL);
	}
}

static void registry_handle_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
	(void)data;
	(void)registry;
	(void)name;
}

static const struct wl_registry_listener registry_listener = {
	.global = registry_handle_global,
	.global_remove = registry_handle_global_remove,
};

static int create_shm_file(size_t size) {
	int fd = memfd_create("wayland-shm", MFD_CLOEXEC | MFD_ALLOW_SEALING);
	if (fd < 0) {
		char template[] = "/tmp/wayland-shm-XXXXXX";
		fd = mkstemp(template);
		if (fd >= 0) unlink(template);
	}
	if (fd < 0) return -1;
	if (ftruncate(fd, size) < 0) {
		close(fd);
		return -1;
	}
	return fd;
}

static int init_buffer(void) {
	int stride = app.width * 4;
	int size = stride * app.height;

	int fd = create_shm_file(size);
	if (fd < 0) {
		perror("Lỗi tạo shm file");
		return -1;
	}

	app.pixel_data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (app.pixel_data == MAP_FAILED) {
		perror("Lỗi mmap buffer");
		close(fd);
		return -1;
	}

	struct wl_shm_pool *pool = wl_shm_create_pool(app.shm, fd, size);
	app.buffer = wl_shm_pool_create_buffer(pool, 0, app.width, app.height, stride, WL_SHM_FORMAT_XRGB8888);
	wl_shm_pool_destroy(pool);
	close(fd);

	return 0;
}

// ==================== TỰ ĐỘNG KHỞI TẠO WESTON VÀ KẾT NỐI SOCKET ====================
static void init_wayland_environment(void) {
	if (!getenv("XDG_RUNTIME_DIR")) {
		setenv("XDG_RUNTIME_DIR", "/tmp/runtime-root", 1);
	}
	mkdir("/tmp/runtime-root", 0700);

	// Kiểm tra nếu Weston chưa chạy thì tự động khởi chạy ngầm
	if (system("pgrep weston > /dev/null 2>&1") != 0) {
		printf("[Wayland App] Không tìm thấy Weston. Đang tự động khởi chạy Weston...\n");
		system("export WESTON_DISABLE_ATOMIC=1; export XDG_RUNTIME_DIR=/tmp/runtime-root; "
		       "mkdir -p -m 0700 /tmp/runtime-root; "
		       "weston --backend=drm-backend.so --use-pixman --continue-without-input > /tmp/weston.log 2>&1 &");

		// Chờ socket sẵn sàng (tối đa 3 giây)
		for (int i = 0; i < 30; i++) {
			usleep(100000);
			if (access("/tmp/runtime-root/wayland-0", F_OK) == 0 ||
			    access("/tmp/runtime-root/wayland-1", F_OK) == 0) {
				break;
			}
		}
	}
}

static struct wl_display *auto_connect_wayland_display(void) {
	struct wl_display *disp = NULL;

	// 1. Thử socket /run/wayland-0 (do systemd weston.service quản lý)
	if (access("/run/wayland-0", F_OK) == 0) {
		setenv("XDG_RUNTIME_DIR", "/run", 1);
		disp = wl_display_connect("wayland-0");
		if (disp) {
			printf("[Wayland App] Kết nối thành công tới: /run/wayland-0\n");
			return disp;
		}
	}

	// 2. Thử socket trong /tmp/runtime-root (nếu chạy weston thủ công)
	setenv("XDG_RUNTIME_DIR", "/tmp/runtime-root", 1);
	if (access("/tmp/runtime-root/wayland-1", F_OK) == 0) {
		disp = wl_display_connect("wayland-1");
		if (disp) {
			printf("[Wayland App] Kết nối thành công tới: /tmp/runtime-root/wayland-1\n");
			return disp;
		}
	}
	if (access("/tmp/runtime-root/wayland-0", F_OK) == 0) {
		disp = wl_display_connect("wayland-0");
		if (disp) {
			printf("[Wayland App] Kết nối thành công tới: /tmp/runtime-root/wayland-0\n");
			return disp;
		}
	}

	// 3. Thử fallback mặc định
	disp = wl_display_connect(NULL);
	return disp;
}

// ==================== MAIN ====================
int main(void) {
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sigint_handler;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	printf("0. Chuẩn bị môi trường hiển thị Weston...\n");
	init_wayland_environment();

	printf("1. Khởi động luồng đọc dữ liệu MPU6050...\n");
	if (pthread_create(&g_mpu_thread, NULL, mpu6050_worker_thread, NULL) != 0) {
		perror("Lỗi tạo luồng mpu6050");
		return 1;
	}

	printf("2. Kết nối tới Wayland Compositor...\n");
	app.display = auto_connect_wayland_display();
	if (!app.display) {
		fprintf(stderr, "Lỗi: Không thể kết nối tới Wayland Display! (Kiểm tra /tmp/weston.log)\n");
		app.running = false;
		pthread_join(g_mpu_thread, NULL);
		return 1;
	}

	app.registry = wl_display_get_registry(app.display);
	wl_registry_add_listener(app.registry, &registry_listener, NULL);
	wl_display_roundtrip(app.display);

	if (!app.compositor || !app.shm || !app.wm_base) {
		fprintf(stderr, "Lỗi: Thiếu interface bắt buộc từ Wayland!\n");
		app.running = false;
		pthread_join(g_mpu_thread, NULL);
		wl_display_disconnect(app.display);
		return 1;
	}

	if (init_buffer() < 0) {
		app.running = false;
		pthread_join(g_mpu_thread, NULL);
		wl_display_disconnect(app.display);
		return 1;
	}

	app.surface = wl_compositor_create_surface(app.compositor);
	app.xdg_surface = xdg_wm_base_get_xdg_surface(app.wm_base, app.surface);
	xdg_surface_add_listener(app.xdg_surface, &xdg_surface_listener, NULL);

	app.xdg_toplevel = xdg_surface_get_toplevel(app.xdg_surface);
	xdg_toplevel_add_listener(app.xdg_toplevel, &xdg_toplevel_listener, NULL);
	xdg_toplevel_set_title(app.xdg_toplevel, "BBB MPU6050 Dashboard");
	xdg_toplevel_set_app_id(app.xdg_toplevel, "bbb.mpu6050.dashboard");

	wl_surface_commit(app.surface);

	printf("3. Dashboard Wayland đang hiển thị (980x680)... Bấm Ctrl+C để thoát.\n");
	while (app.running) {
		if (wl_display_dispatch(app.display) == -1) {
			break;
		}
	}

	printf("\nĐang đóng ứng dụng và dọn dẹp tài nguyên...\n");
	app.running = false;
	pthread_join(g_mpu_thread, NULL);

	if (app.buffer) wl_buffer_destroy(app.buffer);
	if (app.xdg_toplevel) xdg_toplevel_destroy(app.xdg_toplevel);
	if (app.xdg_surface) xdg_surface_destroy(app.xdg_surface);
	if (app.surface) wl_surface_destroy(app.surface);
	if (app.wm_base) xdg_wm_base_destroy(app.wm_base);
	if (app.shm) wl_shm_destroy(app.shm);
	if (app.compositor) wl_compositor_destroy(app.compositor);
	if (app.registry) wl_registry_destroy(app.registry);
	wl_display_disconnect(app.display);

	printf("Hoàn tất.\n");
	return 0;
}
