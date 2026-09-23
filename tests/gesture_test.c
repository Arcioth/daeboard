#include "gesture.h"
#include "ron.h"
#include "sock.h"

#include <stdio.h>
#include <string.h>

static int fails;

static void expect(int cond, const char *msg)
{
	if (!cond) {
		fprintf(stderr, "FAIL %s\n", msg);
		fails++;
	}
}

static void test_meta_and_enter(void)
{
	struct gstate s;
	struct gwrite w[8];
	int timer = 0;
	int n;

	g_init(&s, 204, 255, 254, 3);
	n = g_key(&s, DB_KEY_LEFTMETA, 1, 0, w, 8, &timer);
	expect(g_owner(&s, 0) == G_META, "meta owns on press");
	expect(g_meta_since(&s) == 0, "meta start is the press");
	expect(n >= 1 && w[0].kind == GW_MODE && w[0].mode == 1, "meta writes breathe");
	expect(w[0].r == 204 && w[0].g == 255 && w[0].b == 254, "breathe uses normal");

	n = g_key(&s, DB_KEY_RIGHTMETA, 1, 20, w, 8, &timer);
	expect(g_meta_since(&s) == 0, "second meta does not restart");
	expect(n == 0, "second meta writes nothing");

	n = g_key(&s, DB_KEY_LEFTMETA, 0, 30, w, 8, &timer);
	expect(g_owner(&s, 30) == G_META, "one meta still held");

	n = g_key(&s, DB_KEY_ENTER, 1, 100, w, 8, &timer);
	expect(n >= 4, "enter emits four brightness edges");
	expect(w[0].kind == GW_BRIGHT && w[0].brightness == 0, "enter edge 0");
	expect(w[1].kind == GW_BRIGHT && w[1].brightness == 3, "enter edge 1");
	expect(w[2].kind == GW_BRIGHT && w[2].brightness == 0, "enter edge 2");
	expect(w[3].kind == GW_BRIGHT && w[3].brightness == 3, "enter edge 3");
	expect(g_owner(&s, 100) == G_META, "meta owns again after the blink");
	expect(g_meta_since(&s) == 0, "meta start did not move");

	n = g_key(&s, DB_KEY_RIGHTMETA, 0, 150, w, 8, &timer);
	expect(g_owner(&s, 150) == G_NONE, "meta release returns to idle");
	expect(n >= 1 && w[0].kind == GW_MODE && w[0].mode == 0, "idle writes static");
	expect(w[0].r == 204 && w[0].g == 255 && w[0].b == 254, "static is normal");
}

static void test_enter_restart(void)
{
	struct gstate s;
	struct gwrite w[8];
	int timer = 0;
	int n;

	g_init(&s, 1, 2, 3, 2);
	n = g_key(&s, DB_KEY_ENTER, 1, 0, w, 8, &timer);
	expect(n >= 4 && w[1].brightness == 2, "first blink uses brightness 2");
	n = g_key(&s, DB_KEY_ENTER, 1, 10, w, 8, &timer);
	expect(n >= 4, "second enter blinks again");
	expect(w[0].brightness == 0 && w[3].brightness == 2, "second blink is a full pair");
	expect(g_owner(&s, 10) == G_NONE, "enter does not stay owner");
}

static void test_erase_preempt(void)
{
	struct gstate s;
	struct gwrite w[8];
	int timer = 0;
	int n;

	g_init(&s, 10, 20, 30, 1);
	g_key(&s, DB_KEY_BACKSPACE, 1, 0, w, 8, &timer);
	expect(g_erase_since(&s) == 0, "erase starts at the press");
	g_key(&s, DB_KEY_DELETE, 1, 500, w, 8, &timer);
	expect(g_erase_since(&s) == 0, "delete does not reset the ramp");
	expect(g_owner(&s, 500) == G_ERASE, "erase owns before meta");

	g_key(&s, DB_KEY_LEFTMETA, 1, 4000, w, 8, &timer);
	expect(g_owner(&s, 4000) == G_META, "newer meta hides the ramp");
	expect(w[0].mode == 1, "meta still breathes");

	n = g_key(&s, DB_KEY_LEFTMETA, 0, 6000, w, 8, &timer);
	expect(g_owner(&s, 6000) == G_ERASE, "erase owns after meta");
	expect(n >= 1 && w[0].kind == GW_MODE && w[0].mode == 0, "ramp is static mode");
	expect(w[0].r == 255 && w[0].g == 0 && w[0].b == 0, "six seconds is full red");
	expect(timer == 0, "full red disarms the timer");
}

static void test_hidden_fade_finishes(void)
{
	struct gstate s;
	struct gwrite w[8];
	int timer = 0;
	int n;

	g_init(&s, 10, 20, 30, 2);
	g_key(&s, DB_KEY_BACKSPACE, 1, 0, w, 8, &timer);
	g_key(&s, DB_KEY_LEFTMETA, 1, 1000, w, 8, &timer);
	g_key(&s, DB_KEY_BACKSPACE, 0, 1500, w, 8, &timer);
	expect(g_owner(&s, 1500) == G_META, "release stays hidden under meta");
	n = g_key(&s, DB_KEY_LEFTMETA, 0, 4000, w, 8, &timer);
	expect(g_owner(&s, 4000) == G_NONE, "fade already finished");
	expect(n >= 1 && w[0].kind == GW_MODE && w[0].mode == 0, "result is static");
	expect(w[0].r == 10 && w[0].g == 20 && w[0].b == 30, "result is normal, not a fade frame");
}

static void test_ignore(void)
{
	struct gstate s;
	struct gwrite w[8];
	int timer = 99;
	int n;

	g_init(&s, 1, 2, 3, 1);
	n = g_key(&s, 30, 1, 0, w, 8, &timer);
	expect(n == 0 && timer == -1, "unwatched key is dropped");
	expect(g_owner(&s, 0) == G_NONE, "unwatched key owns nothing");
	g_key(&s, DB_KEY_LEFTMETA, 1, 0, w, 8, &timer);
	n = g_key(&s, DB_KEY_LEFTMETA, 2, 40, w, 8, &timer);
	expect(n == 0 && timer == -1, "repeat is dropped");
	expect(g_meta_since(&s) == 0, "repeat does not move the start");
	expect(g_owner(&s, 40) == G_META, "repeat does not release meta");
}

static void test_ron_and_socket(void)
{
	const char *text =
		"brightness: High,\n"
		"builtins: {\n"
		"Static: ( zone: r#None, colour1: ( r: 204, g: 255, b: 254, ), ),\n"
		"}\n";
	int r = 0, g = 0, b = 0, bri = -1;
	char reply[16];
	int quit = 1;

	expect(ron_parse(text, &r, &g, &b, &bri) == 0, "ron parses");
	expect(r == 204 && g == 255 && b == 254 && bri == 3, "ron values");
	expect(ron_parse("nope", &r, &g, &b, &bri) != 0, "ron rejects junk");

	r = 1;
	g = 2;
	b = 3;
	bri = 1;
	sock_command("ping\n", reply, 16, &r, &g, &b, &bri, &quit);
	expect(strcmp(reply, "pong") == 0 && quit == 0, "ping");
	expect(r == 1 && bri == 1, "ping does not change color");

	sock_command("color ccfffe", reply, 16, &r, &g, &b, &bri, &quit);
	expect(strcmp(reply, "ok") == 0, "color ok");
	expect(r == 204 && g == 255 && b == 254 && bri == 1, "color sets rgb only");

	sock_command("brightness 0", reply, 16, &r, &g, &b, &bri, &quit);
	expect(strcmp(reply, "ok") == 0 && bri == 0, "brightness 0");
	sock_command("brightness 9", reply, 16, &r, &g, &b, &bri, &quit);
	expect(strcmp(reply, "err") == 0 && bri == 0, "brightness rejects 9");
	sock_command("quit", reply, 16, &r, &g, &b, &bri, &quit);
	expect(quit == 1 && strcmp(reply, "ok") == 0, "quit");
	sock_command("nope", reply, 16, &r, &g, &b, &bri, &quit);
	expect(strcmp(reply, "err") == 0, "unknown command");
}

int main(void)
{
	test_meta_and_enter();
	test_enter_restart();
	test_erase_preempt();
	test_hidden_fade_finishes();
	test_ignore();
	test_ron_and_socket();
	if (fails) {
		fprintf(stderr, "%d failed\n", fails);
		return 1;
	}
	printf("ok\n");
	return 0;
}
