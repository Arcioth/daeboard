#include "input.h"

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int input_open_keyboard(void)
{
	DIR *d;
	struct dirent *de;

	d = opendir("/sys/class/input");
	if (!d)
		return -1;
	while ((de = readdir(d)) != NULL) {
		char path[512];
		char name[256];
		int fd, n;

		if (strncmp(de->d_name, "event", 5) != 0)
			continue;
		snprintf(path, sizeof path, "/sys/class/input/%s/device/name", de->d_name);
		fd = open(path, O_RDONLY | O_CLOEXEC);
		if (fd < 0)
			continue;
		n = (int)read(fd, name, sizeof name - 1);
		close(fd);
		if (n <= 0)
			continue;
		if (name[n - 1] == '\n')
			n--;
		name[n] = 0;
		if (strcmp(name, "AT Translated Set 2 keyboard") != 0)
			continue;
		snprintf(path, sizeof path, "/dev/input/%s", de->d_name);
		closedir(d);
		return open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	}
	closedir(d);
	return -1;
}
