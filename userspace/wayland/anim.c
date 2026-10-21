#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <time.h>

int main(void) {
	// Ẩn con trỏ và xóa màn hình
	printf("\033[?25l\033[2J");
	fflush(stdout);

	int w = 45, h = 16;
	float ball_x = 5.0f, ball_y = 5.0f;
	float vx = 1.0f, vy = 0.7f;
	unsigned long frame = 0;

	// ~30 FPS (33ms)
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 33000000 };

	while (1) {
		frame++;
		ball_x += vx;
		ball_y += vy;

		if (ball_x <= 1.0f) { ball_x = 1.0f; vx = -vx; }
		if (ball_x >= w - 2) { ball_x = w - 2; vx = -vx; }
		if (ball_y <= 1.0f) { ball_y = 1.0f; vy = -vy; }
		if (ball_y >= h - 2) { ball_y = h - 2; vy = -vy; }

		// Về đầu màn hình
		printf("\033[H");
		printf("\033[1;36m=== WAYLAND FRAME TEST === Frame: %-6lu\033[0m\n", frame);

		for (int y = 0; y < h; y++) {
			for (int x = 0; x < w; x++) {
				if (x == 0 || x == w - 1 || y == 0 || y == h - 1) {
					printf("\033[1;37m#\033[0m");
				} else if ((int)ball_x == x && (int)ball_y == y) {
					printf("\033[1;31m●\033[0m"); // Quả bóng đỏ nhảy qua lại
				} else {
					// Dải sóng di chuyển
					float wave = sinf(x * 0.35f + frame * 0.15f) * 3.5f + (h / 2.0f);
					if ((int)wave == y) {
						printf("\033[1;32m~\033[0m"); // Làn sóng xanh di chuyển
					} else {
						putchar(' ');
					}
				}
			}
			putchar('\n');
		}
		printf("\033[1;33mSurface rendering: ACTIVE (30 FPS)\033[0m\n");
		fflush(stdout);

		nanosleep(&ts, NULL);
	}
	return 0;
}
