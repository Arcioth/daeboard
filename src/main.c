#define _GNU_SOURCE
#include "gesture.h"
#include "macro.h"
#include "input.h"
#include "led.h"
#include "ron.h"
#include "sock.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/file.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>
#include <linux/input.h>

#define RON_PATH "/etc/asusd/aura_tuf.ron"
#define LOCK_PATH "/run/daeboard/daeboard.lock"
#define MAXW 8

static int epfd = -1;
static int sigfd = -1;
static int tfd = -1;
static int kbd = -1;
static int lfd = -1;
static int lockfd = -1;
static int clients[SOCK_CLIENTS];
static int nclients;
static int fail_streak;
static struct mrun run;
static char binds_path[512];
static int want_ms;

static long long now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

static int add_fd(int fd)
{
	struct epoll_event ev;

	memset(&ev, 0, sizeof ev);
	ev.events = EPOLLIN;
	ev.data.fd = fd;
	return epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
}

static void arm_timer(int ms)
{
	struct itimerspec its;

	memset(&its, 0, sizeof its);
	if (ms < 0)
		ms = 0;
	if (kbd < 0 && ms == 0)
		ms = 1000;
	want_ms = ms;
	if (ms > 0) {
		its.it_value.tv_sec = ms / 1000;
		its.it_value.tv_nsec = (long)(ms % 1000) * 1000000L;
		its.it_interval = its.it_value;
	}
	timerfd_settime(tfd, 0, &its, NULL);
}

static void apply_writes(struct gwrite *w, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		int rc;

		if (w[i].kind == GW_MODE)
			rc = led_mode(w[i].mode, w[i].r, w[i].g, w[i].b, w[i].speed);
		else if (w[i].kind == GW_BRIGHT)
			rc = led_brightness(w[i].brightness);
		else
			continue;
		if (rc != 0) {
			if (fail_streak == 0)
				fprintf(stderr, "daeboard: led write failed: %s\n", strerror(errno));
			fail_streak++;
		} else {
			fail_streak = 0;
		}
	}
}

static void after_step(int n, struct gwrite *w, int timer)
{
	if (n > 0)
		apply_writes(w, n);
	if (timer >= 0)
		arm_timer(timer);
}

static void on_key(int code, int value)
{
	struct gwrite w[MAXW];
	int timer = -1;
	int n;

	n = mrun_key(&run, code, value, now_ms(), w, MAXW, &timer);
	if (run.pending_fire[0]) {
		char msg[40];
		int c;

		snprintf(msg, sizeof msg, "fire %s", run.pending_fire);
		run.pending_fire[0] = 0;
		for (c = 0; c < nclients; c++)
			(void)send(clients[c], msg, strlen(msg), MSG_NOSIGNAL);
	}
	after_step(n, w, timer);
}

static void on_tick(void)
{
	struct gwrite w[MAXW];
	int timer = 0;
	int n;
	unsigned long long expir;
	int got;

	got = (int)read(tfd, &expir, sizeof expir);
	(void)got;
	if (kbd < 0) {
		kbd = input_open_keyboard();
		if (kbd >= 0)
			add_fd(kbd);
	}
	n = mrun_tick(&run, now_ms(), w, MAXW, &timer);
	after_step(n, w, timer);
}

static void read_keys(void)
{
	struct input_event ev;

	for (;;) {
		int n = (int)read(kbd, &ev, sizeof ev);

		if (n < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				return;
			if (errno == ENODEV || errno == EIO) {
				epoll_ctl(epfd, EPOLL_CTL_DEL, kbd, NULL);
				close(kbd);
				kbd = -1;
				on_key(DB_KEY_LEFTMETA, 0);
				on_key(DB_KEY_RIGHTMETA, 0);
				on_key(DB_KEY_BACKSPACE, 0);
				on_key(DB_KEY_DELETE, 0);
				arm_timer(want_ms);
				return;
			}
			return;
		}
		if (n != (int)sizeof ev)
			continue;
		if (ev.type != EV_KEY)
			continue;
		on_key(ev.code, ev.value);
	}
}

static void drop_client(int fd)
{
	int i;

	epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
	close(fd);
	for (i = 0; i < nclients; i++) {
		if (clients[i] != fd)
			continue;
		clients[i] = clients[nclients - 1];
		nclients--;
		return;
	}
}

static void restore_normal(void)
{
	led_mode(0, run.nr, run.ng, run.nb, 1);
	led_brightness(run.nbri);
}

static void reload_binds(void)
{
	struct mtable t;
	int rc;

	if (!binds_path[0])
		return;
	rc = macro_load(binds_path, &t);
	if (rc == 0) {
		mrun_set_table(&run, &t);
		return;
	}
	if (rc == -2)
		fprintf(stderr, "daeboard: cannot read %s\n", binds_path);
	else
		fprintf(stderr, "daeboard: %s:%d\n", binds_path, t.err_line);
}

static void reload_ron(void)
{
	int r, g, b, bri;

	if (ron_load(RON_PATH, &r, &g, &b, &bri) != 0) {
		fprintf(stderr, "daeboard: could not read %s\n", RON_PATH);
		return;
	}
	mrun_set_normal(&run, r, g, b, bri);
	if (mrun_idle(&run)) {
		led_mode(0, run.nr, run.ng, run.nb, 1);
		led_brightness(run.nbri);
	}
}

static int handle_signal(void)
{
	struct signalfd_siginfo info;
	int n;

	n = (int)read(sigfd, &info, sizeof info);
	if (n != (int)sizeof info)
		return 0;
	if (info.ssi_signo == SIGHUP) {
		reload_ron();
		reload_binds();
		return 0;
	}
	if (info.ssi_signo == SIGINT || info.ssi_signo == SIGTERM)
		return 1;
	return 0;
}

static void take_client(void)
{
	int fd;
	char reply[16];

	fd = accept4(lfd, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
	if (fd < 0)
		return;
	if (nclients >= SOCK_CLIENTS) {
		snprintf(reply, sizeof reply, "err");
		(void)send(fd, reply, strlen(reply), MSG_NOSIGNAL);
		close(fd);
		return;
	}
	if (add_fd(fd) != 0) {
		close(fd);
		return;
	}
	clients[nclients++] = fd;
}

static int client_line(int fd)
{
	char buf[64];
	char reply[16];
	int n, quit = 0;
	int r, g, b, bri;
	int owner;

	n = (int)recv(fd, buf, sizeof buf, 0);
	if (n <= 0) {
		drop_client(fd);
		return 0;
	}
	if (n == (int)sizeof buf) {
		snprintf(reply, sizeof reply, "err");
		(void)send(fd, reply, strlen(reply), MSG_NOSIGNAL);
		return 0;
	}
	buf[n] = 0;
	if (strncmp(buf, "reload", 6) == 0 &&
	    (buf[6] == 0 || buf[6] == '\n' || buf[6] == '\r')) {
		struct mtable t;
		int rc = binds_path[0] ? macro_load(binds_path, &t) : -2;

		if (rc == 0) {
			mrun_set_table(&run, &t);
			snprintf(reply, sizeof reply, "ok");
		} else if (rc == -1) {
			snprintf(reply, sizeof reply, "err %d", t.err_line);
		} else {
			snprintf(reply, sizeof reply, "err");
		}
		(void)send(fd, reply, strlen(reply), MSG_NOSIGNAL);
		return 0;
	}
	r = run.nr;
	g = run.ng;
	b = run.nb;
	bri = run.nbri;
	sock_command(buf, reply, (int)sizeof reply, &r, &g, &b, &bri, &quit);
	(void)send(fd, reply, strlen(reply), MSG_NOSIGNAL);
	if (quit)
		return 1;
	if (strcmp(reply, "ok") != 0)
		return 0;
	mrun_set_normal(&run, r, g, b, bri);
	owner = mrun_idle(&run);
	if (owner) {
		led_mode(0, run.nr, run.ng, run.nb, 1);
		led_brightness(run.nbri);
	}
	return 0;
}

static int setup_signals(void)
{
	sigset_t set;

	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGTERM);
	sigaddset(&set, SIGHUP);
	if (sigprocmask(SIG_BLOCK, &set, NULL) != 0)
		return -1;
	sigfd = signalfd(-1, &set, SFD_NONBLOCK | SFD_CLOEXEC);
	return sigfd;
}

static int take_lock(void)
{
	if (mkdir("/run/daeboard", 0755) != 0 && errno != EEXIST)
		return -1;
	lockfd = open(LOCK_PATH, O_CREAT | O_RDWR | O_CLOEXEC, 0644);
	if (lockfd < 0)
		return -1;
	if (flock(lockfd, LOCK_EX | LOCK_NB) != 0)
		return -1;
	return 0;
}

static void load_normal(void)
{
	int r = 204, g = 255, b = 254, bri = 3;
	int sysbri;
	FILE *f;
	char buf[16];

	if (ron_load(RON_PATH, &r, &g, &b, &bri) != 0) {
		f = fopen("/sys/class/leds/asus::kbd_backlight/brightness", "r");
		if (f) {
			if (fgets(buf, sizeof buf, f))
				sysbri = atoi(buf);
			else
				sysbri = 3;
			fclose(f);
			if (sysbri >= 0 && sysbri <= 3)
				bri = sysbri;
		}
	}
	mrun_init(&run, r, g, b, bri);
}

int main(int argc, char **argv)
{
	struct epoll_event evs[8];
	int i;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--binds") == 0 && i + 1 < argc) {
			snprintf(binds_path, sizeof binds_path, "%s", argv[++i]);
			continue;
		}
		fprintf(stderr, "usage: daeboard [--binds FILE]\n");
		return 1;
	}
	signal(SIGPIPE, SIG_IGN);
	if (take_lock() != 0) {
		if (errno == EWOULDBLOCK || errno == EAGAIN)
			fprintf(stderr, "daeboard: already running\n");
		else
			fprintf(stderr, "daeboard: cannot lock %s: %s\n", LOCK_PATH, strerror(errno));
		return 1;
	}
	load_normal();
	reload_binds();
	epfd = epoll_create1(EPOLL_CLOEXEC);
	tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
	if (epfd < 0 || tfd < 0 || setup_signals() < 0) {
		fprintf(stderr, "daeboard: epoll setup failed: %s\n", strerror(errno));
		return 1;
	}
	lfd = sock_listen();
	if (lfd < 0) {
		fprintf(stderr, "daeboard: socket setup failed: %s\n", strerror(errno));
		return 1;
	}
	add_fd(sigfd);
	add_fd(tfd);
	add_fd(lfd);
	kbd = input_open_keyboard();
	if (kbd >= 0)
		add_fd(kbd);
	else
		fprintf(stderr, "daeboard: keyboard missing, retrying\n");
	arm_timer(0);

	for (;;) {
		int n = epoll_wait(epfd, evs, 8, -1);
		int stop = 0;

		if (n < 0) {
			if (errno == EINTR)
				continue;
			fprintf(stderr, "daeboard: epoll: %s\n", strerror(errno));
			break;
		}
		for (i = 0; i < n; i++) {
			int fd = evs[i].data.fd;

			if (fd == sigfd)
				stop = handle_signal();
			else if (fd == tfd)
				on_tick();
			else if (fd == kbd)
				read_keys();
			else if (fd == lfd)
				take_client();
			else
				stop = client_line(fd);
			if (stop)
				break;
		}
		if (stop)
			break;
	}

	restore_normal();
	unlink(SOCK_PATH);
	return 0;
}
