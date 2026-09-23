#include "macro.h"

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

static void test_compile(void)
{
	struct mtable t;
	const char *text =
		"[enter]\n"
		"down = color 000000 120, color ccfffe 120\n"
		"[backspace]\n"
		"down = color ff0000 0\n"
		"up = fade 2000\n"
		"[f6]\n"
		"ctron = profile performance\n"
		"[leftmeta]\n"
		"down = breathe 0\n"
		"up = rest\n";

	expect(macro_compile(text, &t) == 0, "compile example");
	expect(t.n == 4 && t.b[2].has_ctron && t.b[2].n_down == 0, "f6 is a fire-only bind");
	expect(strcmp(t.b[3].name, "leftmeta") == 0, "section name is kept");
	expect(t.b[0].code == DB_KEY_ENTER && t.b[0].n_down == 2, "enter has two steps");
	expect(t.b[0].down[0].op == M_COLOR && t.b[0].down[0].ms == 120, "first pause is 120");
	expect(t.b[0].down[1].r == 204 && t.b[0].down[1].g == 255 && t.b[0].down[1].b == 254,
	       "second color is ice");
	expect(t.b[1].n_up == 1 && t.b[1].up[0].op == M_FADE && t.b[1].up[0].ms == 2000,
	       "backspace release fades");
	expect(t.b[3].down[0].op == M_BREATHE && t.b[3].down[0].ms == 0, "meta holds");
	expect(macro_compile("[nope]\ndown = color ff0000 0\n[enter]\ndown = rest\n", &t) == 0,
	       "unknown name is skipped");
	expect(t.n == 1 && t.b[0].code == DB_KEY_ENTER, "later keys still load");
	expect(macro_compile("[enter]\ndown = color zz0000 10\n", &t) != 0 && t.err_line == 2,
	       "bad color names the line");
}

static void test_builtin_enter_and_red(void)
{
	struct mrun r;
	struct gwrite w[8];
	int timer = 0;
	int n;

	mrun_init(&r, 204, 255, 254, 3);
	n = mrun_key(&r, DB_KEY_ENTER, 1, 0, w, 8, &timer);
	expect(n == 1 && w[0].kind == GW_BRIGHT && w[0].brightness == 0, "enter starts off");
	expect(timer == 120, "enter holds the edge");
	mrun_key(&r, DB_KEY_ENTER, 0, 5, w, 8, &timer);
	n = mrun_tick(&r, 120, w, 8, &timer);
	expect(n == 1 && w[0].brightness == 3, "second edge on");
	n = mrun_tick(&r, 240, w, 8, &timer);
	expect(w[0].brightness == 0, "third edge off");
	n = mrun_tick(&r, 360, w, 8, &timer);
	expect(w[0].brightness == 3, "fourth edge on");
	n = mrun_tick(&r, 480, w, 8, &timer);
	expect(mrun_idle(&r), "blink ends");

	n = mrun_key(&r, DB_KEY_BACKSPACE, 1, 1000, w, 8, &timer);
	expect(n >= 1 && w[0].kind == GW_MODE && w[0].r == 255 && w[0].g == 0 && w[0].b == 0,
	       "backspace is red immediately");
	expect(timer == 0, "red holds without a timer");
	n = mrun_key(&r, DB_KEY_BACKSPACE, 0, 1100, w, 8, &timer);
	expect(n >= 1 && w[0].kind == GW_MODE, "release starts the fade");
	expect(timer > 0 && timer <= 100, "fade ticks");
}

static void test_zero_pause_does_not_stall(void)
{
	struct mtable t;
	struct mrun r;
	struct gwrite w[8];
	int timer = 0;
	const char *text =
		"[key6]\n"
		"down = color ffffff 200\n"
		"[space]\n"
		"down = color ff8800 0, brightness 3 120\n";

	expect(macro_compile(text, &t) == 0, "unknown name does not reject the file");
	expect(t.n == 1 && t.b[0].code == 57, "space is loaded");
	expect(t.b[0].n_down == 2, "both space steps are kept");
	mrun_init(&r, 1, 2, 3, 3);
	mrun_set_table(&r, &t);
	mrun_key(&r, 57, 1, 0, w, 8, &timer);
	expect(w[0].kind == GW_BRIGHT && w[0].brightness == 3,
	       "a 0 pause in front does not freeze the macro");
}

static void test_meta_covers_enter(void)
{
	struct mrun r;
	struct gwrite w[8];
	int timer = 0;

	mrun_init(&r, 10, 20, 30, 2);
	mrun_key(&r, DB_KEY_ENTER, 1, 0, w, 8, &timer);
	mrun_key(&r, DB_KEY_LEFTMETA, 1, 10, w, 8, &timer);
	expect(w[0].kind == GW_MODE && w[0].mode == 1, "newer meta breathes");
	mrun_key(&r, DB_KEY_ENTER, 0, 20, w, 8, &timer);
	mrun_key(&r, DB_KEY_LEFTMETA, 0, 5000, w, 8, &timer);
	expect(mrun_idle(&r), "enter finished while meta was held");
}

int main(void)
{
	test_compile();
	test_builtin_enter_and_red();
	test_zero_pause_does_not_stall();
	test_meta_covers_enter();
	if (fails) {
		fprintf(stderr, "%d failed\n", fails);
		return 1;
	}
	printf("ok\n");
	return 0;
}
