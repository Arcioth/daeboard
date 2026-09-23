#include "sock.h"

#include <errno.h>
#include <grp.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

static int hexval(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

int sock_listen(void)
{
	struct sockaddr_un addr;
	struct group *gr;
	int fd;

	if (mkdir("/run/daeboard", 0755) != 0 && errno != EEXIST)
		return -1;
	unlink(SOCK_PATH);
	fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
	if (fd < 0)
		return -1;
	memset(&addr, 0, sizeof addr);
	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path, sizeof addr.sun_path, "%s", SOCK_PATH);
	if (bind(fd, (struct sockaddr *)&addr, sizeof addr) != 0) {
		close(fd);
		return -1;
	}
	chmod(SOCK_PATH, 0660);
	gr = getgrnam("wheel");
	if (gr && chown(SOCK_PATH, 0, gr->gr_gid) != 0)
		fprintf(stderr, "daeboard: chown %s: %s\n", SOCK_PATH, strerror(errno));
	if (listen(fd, SOCK_CLIENTS) != 0) {
		close(fd);
		unlink(SOCK_PATH);
		return -1;
	}
	return fd;
}

int sock_command(const char *line, char *reply, int reply_max,
		 int *r, int *g, int *b, int *bri, int *quit)
{
	char tmp[64];
	int i, n;

	*quit = 0;
	n = (int)strlen(line);
	while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r' || line[n - 1] == ' '))
		n--;
	if (n <= 0 || n >= (int)sizeof tmp) {
		snprintf(reply, (size_t)reply_max, "err");
		return 0;
	}
	memcpy(tmp, line, (size_t)n);
	tmp[n] = 0;

	if (strcmp(tmp, "ping") == 0) {
		snprintf(reply, (size_t)reply_max, "pong");
		return 0;
	}
	if (strcmp(tmp, "quit") == 0) {
		*quit = 1;
		snprintf(reply, (size_t)reply_max, "ok");
		return 0;
	}
	if (strncmp(tmp, "brightness ", 11) == 0) {
		int v = 0;
		const char *p = tmp + 11;

		if (*p < '0' || *p > '9' || p[1] != 0) {
			snprintf(reply, (size_t)reply_max, "err");
			return 0;
		}
		v = *p - '0';
		if (v > 3) {
			snprintf(reply, (size_t)reply_max, "err");
			return 0;
		}
		*bri = v;
		snprintf(reply, (size_t)reply_max, "ok");
		return 0;
	}
	if (strncmp(tmp, "color ", 6) == 0) {
		const char *p = tmp + 6;
		int rr = 0, gg = 0, bb = 0;

		if (strlen(p) != 6) {
			snprintf(reply, (size_t)reply_max, "err");
			return 0;
		}
		for (i = 0; i < 6; i++) {
			int h = hexval(p[i]);

			if (h < 0) {
				snprintf(reply, (size_t)reply_max, "err");
				return 0;
			}
			if (i < 2)
				rr = rr * 16 + h;
			else if (i < 4)
				gg = gg * 16 + h;
			else
				bb = bb * 16 + h;
		}
		*r = rr;
		*g = gg;
		*b = bb;
		snprintf(reply, (size_t)reply_max, "ok");
		return 0;
	}
	snprintf(reply, (size_t)reply_max, "err");
	return 0;
}
