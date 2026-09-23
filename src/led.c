#include "led.h"

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#define BRIGHT_PATH "/sys/class/leds/asus::kbd_backlight/brightness"
#define MODE_PATH "/sys/class/leds/asus::kbd_backlight/kbd_rgb_mode"

static int clamp(int v, int lo, int hi)
{
	if (v < lo)
		return lo;
	if (v > hi)
		return hi;
	return v;
}

static int write_sysfs(const char *path, const char *buf, int len)
{
	int fd, n;

	fd = open(path, O_WRONLY | O_CLOEXEC);
	if (fd < 0)
		return -1;
	n = (int)write(fd, buf, (size_t)len);
	close(fd);
	if (n != len)
		return -1;
	return 0;
}

int led_brightness(int v)
{
	char buf[16];
	int len;

	v = clamp(v, 0, 3);
	len = snprintf(buf, sizeof buf, "%d", v);
	return write_sysfs(BRIGHT_PATH, buf, len);
}

int led_mode(int mode, int r, int g, int b, int speed)
{
	char buf[64];
	int len;

	r = clamp(r, 0, 255);
	g = clamp(g, 0, 255);
	b = clamp(b, 0, 255);
	speed = clamp(speed, 0, 2);
	/* cmd 0 sets the live color. cmd 1 would save it into the BIOS. */
	len = snprintf(buf, sizeof buf, "0 %d %d %d %d %d\n", mode, r, g, b, speed);
	return write_sysfs(MODE_PATH, buf, len);
}
