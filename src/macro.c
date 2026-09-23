#include "macro.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static const struct {
	const char *name;
	int code;
} key_names[] = {
	{ "esc", 1 }, { "1", 2 }, { "2", 3 }, { "3", 4 }, { "4", 5 },
	{ "5", 6 }, { "6", 7 }, { "7", 8 }, { "8", 9 }, { "9", 10 }, { "0", 11 },
	{ "minus", 12 }, { "equal", 13 }, { "backspace", 14 }, { "tab", 15 },
	{ "q", 16 }, { "w", 17 }, { "e", 18 }, { "r", 19 }, { "t", 20 },
	{ "y", 21 }, { "u", 22 }, { "i", 23 }, { "o", 24 }, { "p", 25 },
	{ "enter", 28 }, { "leftctrl", 29 }, { "a", 30 }, { "s", 31 }, { "d", 32 },
	{ "f", 33 }, { "g", 34 }, { "h", 35 }, { "j", 36 }, { "k", 37 }, { "l", 38 },
	{ "leftshift", 42 }, { "z", 44 }, { "x", 45 }, { "c", 46 }, { "v", 47 },
	{ "b", 48 }, { "n", 49 }, { "m", 50 }, { "rightshift", 54 },
	{ "leftalt", 56 }, { "space", 57 }, { "capslock", 58 },
	{ "f1", 59 }, { "f2", 60 }, { "f3", 61 }, { "f4", 62 }, { "f5", 63 },
	{ "f6", 64 }, { "f7", 65 }, { "f8", 66 }, { "f9", 67 }, { "f10", 68 },
	{ "f11", 87 }, { "f12", 88 }, { "rightctrl", 97 }, { "rightalt", 100 },
	{ "up", 103 }, { "left", 105 }, { "right", 106 }, { "down", 108 },
	{ "delete", 111 }, { "leftmeta", 125 }, { "rightmeta", 126 },
};

static int key_code(const char *s, int n)
{
	char buf[32];
	int i;

	if (n <= 0 || n >= (int)sizeof buf)
		return -1;
	for (i = 0; i < n; i++)
		buf[i] = (char)tolower((unsigned char)s[i]);
	buf[n] = 0;
	if (!strcmp(buf, "del"))
		return 111;
	if (!strcmp(buf, "escape"))
		return 1;
	for (i = 0; i < (int)(sizeof key_names / sizeof key_names[0]); i++) {
		if (!strcmp(buf, key_names[i].name))
			return key_names[i].code;
	}
	return -1;
}

static const char *skip(const char *p, const char *end)
{
	while (p < end && (*p == ' ' || *p == '\t'))
		p++;
	return p;
}

static int take_int(const char **p, const char *end, int *out)
{
	int v = 0, any = 0;
	const char *s = skip(*p, end);

	while (s < end && *s >= '0' && *s <= '9') {
		any = 1;
		v = v * 10 + (*s - '0');
		if (v > 600000)
			return -1;
		s++;
	}
	if (!any)
		return -1;
	*out = v;
	*p = s;
	return 0;
}

static int hexdig(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

static int take_hex(const char **p, const char *end, int *r, int *g, int *b)
{
	const char *s = skip(*p, end);
	int i, v[6];

	if (end - s < 6)
		return -1;
	for (i = 0; i < 6; i++) {
		v[i] = hexdig(s[i]);
		if (v[i] < 0)
			return -1;
	}
	*r = v[0] * 16 + v[1];
	*g = v[2] * 16 + v[3];
	*b = v[4] * 16 + v[5];
	*p = s + 6;
	return 0;
}

static int starts(const char *p, const char *end, const char *w)
{
	int n = (int)strlen(w);

	if (end - p < n)
		return 0;
	if (strncmp(p, w, (size_t)n) != 0)
		return 0;
	if (p + n < end && (isalnum((unsigned char)p[n]) || p[n] == '_'))
		return 0;
	return 1;
}

static int parse_step(const char **p, const char *end, struct mstep *st)
{
	const char *s = skip(*p, end);
	int ms = 0;

	memset(st, 0, sizeof *st);
	if (starts(s, end, "color")) {
		s += 5;
		st->op = M_COLOR;
		if (take_hex(&s, end, &st->r, &st->g, &st->b) || take_int(&s, end, &ms))
			return -1;
		st->ms = ms;
	} else if (starts(s, end, "brightness")) {
		s += 10;
		st->op = M_BRIGHT;
		if (take_int(&s, end, &st->bri) || st->bri > 3 || take_int(&s, end, &ms))
			return -1;
		st->ms = ms;
	} else if (starts(s, end, "breathe")) {
		s += 7;
		st->op = M_BREATHE;
		if (take_int(&s, end, &ms))
			return -1;
		st->ms = ms;
	} else if (starts(s, end, "fade")) {
		s += 4;
		st->op = M_FADE;
		if (take_int(&s, end, &ms) || ms <= 0)
			return -1;
		st->ms = ms;
	} else if (starts(s, end, "rest")) {
		s += 4;
		st->op = M_REST;
		st->ms = 0;
	} else {
		return -1;
	}
	*p = skip(s, end);
	return 0;
}

static int parse_steps(const char *s, const char *end, struct mstep *out, int *n)
{
	*n = 0;
	s = skip(s, end);
	if (s >= end)
		return 0;
	while (s < end) {
		if (*n >= M_STEPS)
			return -1;
		if (parse_step(&s, end, &out[*n]))
			return -1;
		(*n)++;
		s = skip(s, end);
		if (s >= end)
			break;
		if (*s != ',')
			return -1;
		s = skip(s + 1, end);
	}
	return 0;
}

static struct mbind *find_or_add(struct mtable *t, int code)
{
	int i;

	for (i = 0; i < t->n; i++) {
		if (t->b[i].code == code)
			return &t->b[i];
	}
	if (t->n >= M_BINDS)
		return NULL;
	memset(&t->b[t->n], 0, sizeof t->b[t->n]);
	t->b[t->n].code = code;
	return &t->b[t->n++];
}

int macro_compile(const char *text, struct mtable *t)
{
	const char *p = text;
	int line = 1;
	int cur = -1;
	struct mbind *b = NULL;

	memset(t, 0, sizeof *t);
	if (!text)
		return 0;
	while (*p) {
		const char *eol = strchr(p, '\n');
		const char *s, *end;
		int key;

		if (!eol)
			eol = p + strlen(p);
		s = skip(p, eol);
		end = eol;
		while (end > s && (end[-1] == '\r' || end[-1] == ' ' || end[-1] == '\t'))
			end--;
		if (s < end && *s != '#') {
			if (*s == '[') {
				const char *k = s + 1;
				const char *ke = k;

				while (ke < end && *ke != ']')
					ke++;
				if (ke >= end || *ke != ']') {
					t->err_line = line;
					return -1;
				}
				key = key_code(k, (int)(ke - k));
				if (key < 0) {
					/* A name the kernel will never send must not
					 * throw away every later key in the file. */
					b = NULL;
					cur = -2;
					goto next_line;
				}
				b = find_or_add(t, key);
				if (!b) {
					t->err_line = line;
					return -1;
				}
				snprintf(b->name, sizeof b->name, "%.*s",
					 (int)(ke - k) > 23 ? 23 : (int)(ke - k), k);
				cur = key;
			} else if (cur == -2) {
				goto next_line;
			} else if (cur < 0) {
				t->err_line = line;
				return -1;
			} else if (starts(s, end, "ctron")) {
				b->has_ctron = 1;
			} else if (starts(s, end, "down") || starts(s, end, "up")) {
				int up = starts(s, end, "up");
				const char *eq;
				struct mstep *dst;
				int *pn;

				eq = s;
				while (eq < end && *eq != '=')
					eq++;
				if (eq >= end) {
					t->err_line = line;
					return -1;
				}
				dst = up ? b->up : b->down;
				pn = up ? &b->n_up : &b->n_down;
				if (parse_steps(eq + 1, end, dst, pn)) {
					t->err_line = line;
					return -1;
				}
			} else {
				t->err_line = line;
				return -1;
			}
		}
	next_line:
		if (*eol == '\n') {
			p = eol + 1;
			line++;
		} else {
			break;
		}
	}
	{
		int i, j;

		for (i = 0; i < t->n;) {
			if (t->b[i].n_down == 0 && t->b[i].n_up == 0 && !t->b[i].has_ctron) {
				for (j = i + 1; j < t->n; j++)
					t->b[j - 1] = t->b[j];
				t->n--;
			} else {
				i++;
			}
		}
	}
	return 0;
}

void macro_builtin(struct mtable *t)
{
	static const char *text =
		"[leftmeta]\n"
		"down = breathe 0\n"
		"up = rest\n"
		"[rightmeta]\n"
		"down = breathe 0\n"
		"up = rest\n"
		"[enter]\n"
		"down = brightness 0 120, brightness 3 120, brightness 0 120, brightness 3 120\n"
		"[backspace]\n"
		"down = color ff0000 0\n"
		"up = fade 2000\n"
		"[delete]\n"
		"down = color ff0000 0\n"
		"up = fade 2000\n";

	if (macro_compile(text, t) != 0)
		memset(t, 0, sizeof *t);
}

int macro_load(const char *path, struct mtable *t)
{
	FILE *f;
	char buf[8192];
	size_t n;

	f = fopen(path, "r");
	if (!f)
		return -2;
	n = fread(buf, 1, sizeof buf - 1, f);
	fclose(f);
	if (n == sizeof buf - 1) {
		t->err_line = 0;
		return -1;
	}
	buf[n] = 0;
	return macro_compile(buf, t);
}

void mrun_init(struct mrun *r, int red, int grn, int blu, int bri)
{
	memset(r, 0, sizeof *r);
	r->nr = red;
	r->ng = grn;
	r->nb = blu;
	r->nbri = bri;
	r->lit_slot = -1;
	macro_builtin(&r->tab);
}

void mrun_set_normal(struct mrun *r, int red, int grn, int blu, int bri)
{
	r->nr = red;
	r->ng = grn;
	r->nb = blu;
	r->nbri = bri;
}

void mrun_set_table(struct mrun *r, const struct mtable *t)
{
	int i;

	r->tab = *t;
	for (i = 0; i < 8; i++)
		r->used[i] = 0;
	r->lit_slot = -1;
}

int mrun_idle(const struct mrun *r)
{
	int i;

	for (i = 0; i < 8; i++) {
		if (r->used[i])
			return 0;
	}
	return 1;
}

static struct mbind *any_of(struct mrun *r, int code)
{
	int i;

	for (i = 0; i < r->tab.n; i++) {
		if (r->tab.b[i].code == code)
			return &r->tab.b[i];
	}
	return NULL;
}

static struct mbind *bind_of(struct mrun *r, int code)
{
	struct mbind *b = any_of(r, code);

	if (!b || (!b->n_down && !b->n_up))
		return NULL;
	return b;
}

static int slot_of(struct mrun *r, int code)
{
	int i, free_s = -1;

	for (i = 0; i < 8; i++) {
		if (r->used[i] && r->code[i] == code)
			return i;
		if (!r->used[i] && free_s < 0)
			free_s = i;
	}
	return free_s;
}

static const struct mstep *cur_step(struct mrun *r, int s, int *nstep)
{
	struct mbind *b = bind_of(r, r->code[s]);
	const struct mstep *st;
	int n;

	if (!b || !r->used[s]) {
		*nstep = 0;
		return NULL;
	}
	if (r->edge[s] == 0) {
		st = b->down;
		n = b->n_down;
	} else {
		st = b->up;
		n = b->n_up;
	}
	*nstep = n;
	if (r->step[s] < 0 || r->step[s] >= n)
		return NULL;
	return &st[r->step[s]];
}

static void advance_slot(struct mrun *r, int s, long long now)
{
	for (;;) {
		struct mbind *b;
		int n = 0;
		const struct mstep *st;

		if (!r->used[s])
			return;
		b = bind_of(r, r->code[s]);
		if (!b) {
			r->used[s] = 0;
			return;
		}
		st = cur_step(r, s, &n);
		if (!st) {
			if (r->edge[s] == 0 && !r->held[s]) {
				r->edge[s] = 1;
				r->step[s] = 0;
				r->step_at[s] = now;
				if (b->n_up == 0)
					r->used[s] = 0;
				continue;
			}
			if (r->edge[s] == 1)
				r->used[s] = 0;
			return;
		}
		if (st->op == M_REST)
			return;
		/* A 0 pause holds only when it is the last step. Earlier
		 * presets use 0 to mean "set this and stay"; once more
		 * steps were added after it, the list has to keep playing. */
		if (st->ms == 0) {
			if (r->step[s] + 1 < n) {
				r->step[s]++;
				r->step_at[s] = now;
				continue;
			}
			return;
		}
		if (now < r->step_at[s] + st->ms)
			return;
		r->step_at[s] += st->ms;
		r->step[s]++;
	}
}

static int push_mode(struct gwrite *w, int n, int max,
		     int mode, int r, int g, int b, int speed)
{
	if (n >= max)
		return n;
	w[n].kind = GW_MODE;
	w[n].mode = mode;
	w[n].r = r;
	w[n].g = g;
	w[n].b = b;
	w[n].speed = speed;
	w[n].brightness = -1;
	return n + 1;
}

static int push_bri(struct gwrite *w, int n, int max, int bri)
{
	if (n >= max)
		return n;
	w[n].kind = GW_BRIGHT;
	w[n].brightness = bri;
	w[n].mode = 0;
	w[n].r = w[n].g = w[n].b = 0;
	w[n].speed = 0;
	return n + 1;
}

static int lerp(int a, int c, long long num, long long den)
{
	if (den <= 0)
		return c;
	if (num < 0)
		num = 0;
	if (num > den)
		num = den;
	return (int)(a + (long long)(c - a) * num / den);
}

static int owner_slot(struct mrun *r)
{
	int i, best = -1;
	long long since = -1;

	for (i = 0; i < 8; i++) {
		if (!r->used[i])
			continue;
		if (r->since[i] >= since) {
			since = r->since[i];
			best = i;
		}
	}
	return best;
}

static int render(struct mrun *r, long long now, struct gwrite *w, int maxw, int *timer)
{
	int s = owner_slot(r);
	int nstep = 0;
	const struct mstep *st;
	int n = 0;
	int rr, gg, bb, bri;
	long long elapsed, rem;

	*timer = 0;
	if (s < 0) {
		if (r->lit_slot != -2) {
			n = push_mode(w, n, maxw, 0, r->nr, r->ng, r->nb, 1);
			n = push_bri(w, n, maxw, r->nbri);
			r->lit_slot = -2;
		}
		return n;
	}
	st = cur_step(r, s, &nstep);
	if (!st) {
		*timer = 0;
		return 0;
	}
	if (st->op != M_FADE && r->lit_slot == s && r->lit_edge == r->edge[s] &&
	    r->lit_step == r->step[s]) {
		if (st->ms > 0) {
			rem = (r->step_at[s] + st->ms) - now;
			*timer = rem > 0 ? (int)rem : 1;
		}
		return 0;
	}
	switch (st->op) {
	case M_COLOR:
		n = push_mode(w, n, maxw, 0, st->r, st->g, st->b, 1);
		r->fade_r[s] = st->r;
		r->fade_g[s] = st->g;
		r->fade_b[s] = st->b;
		r->fade_bri[s] = r->nbri;
		break;
	case M_BRIGHT:
		n = push_bri(w, n, maxw, st->bri);
		break;
	case M_BREATHE:
		n = push_mode(w, n, maxw, 1, r->nr, r->ng, r->nb, 2);
		n = push_bri(w, n, maxw, r->nbri);
		break;
	case M_REST:
		n = push_mode(w, n, maxw, 0, r->nr, r->ng, r->nb, 1);
		n = push_bri(w, n, maxw, r->nbri);
		r->used[s] = 0;
		r->lit_slot = -2;
		*timer = 0;
		return n;
	case M_FADE:
		elapsed = now - r->step_at[s];
		if (r->lit_slot != s || r->lit_step != r->step[s] || r->lit_edge != r->edge[s]) {
			/* origin is whatever was last painted; first fade uses stored */
		}
		rr = lerp(r->fade_r[s], r->nr, elapsed, st->ms);
		gg = lerp(r->fade_g[s], r->ng, elapsed, st->ms);
		bb = lerp(r->fade_b[s], r->nb, elapsed, st->ms);
		bri = lerp(r->fade_bri[s], r->nbri, elapsed, st->ms);
		n = push_mode(w, n, maxw, 0, rr, gg, bb, 1);
		n = push_bri(w, n, maxw, bri);
		rem = st->ms - elapsed;
		*timer = rem > 100 ? 100 : (rem > 0 ? (int)rem : 1);
		r->lit_slot = s;
		r->lit_edge = r->edge[s];
		r->lit_step = r->step[s];
		return n;
	default:
		break;
	}
	r->lit_slot = s;
	r->lit_edge = r->edge[s];
	r->lit_step = r->step[s];
	if (st->ms > 0) {
		rem = (r->step_at[s] + st->ms) - now;
		*timer = rem > 0 ? (int)rem : 1;
	}
	return n;
}

static void advance_all(struct mrun *r, long long now)
{
	int i;

	for (i = 0; i < 8; i++)
		advance_slot(r, i, now);
}

static int present(struct mrun *r, long long now, struct gwrite *w, int maxw, int *timer)
{
	advance_all(r, now);
	return render(r, now, w, maxw, timer);
}

int mrun_key(struct mrun *r, int code, int value, long long now,
	     struct gwrite *w, int maxw, int *timer_ms)
{
	struct mbind *b;
	int s;

	*timer_ms = -1;
	if (value == 2)
		return 0;
	if (value != 0 && value != 1)
		return 0;
	b = any_of(r, code);
	if (!b)
		return 0;
	if (value == 1 && b->has_ctron)
		snprintf(r->pending_fire, sizeof r->pending_fire, "%s", b->name);
	if (!b->n_down && !b->n_up)
		return 0;
	s = slot_of(r, code);
	if (s < 0)
		return 0;
	if (value == 1) {
		r->used[s] = 1;
		r->code[s] = code;
		r->held[s] = 1;
		r->edge[s] = 0;
		r->step[s] = 0;
		r->step_at[s] = now;
		r->since[s] = now;
		r->fade_r[s] = r->nr;
		r->fade_g[s] = r->ng;
		r->fade_b[s] = r->nb;
		r->fade_bri[s] = r->nbri;
		r->lit_slot = -1;
	} else if (r->used[s]) {
		int nstep = 0;
		const struct mstep *st = cur_step(r, s, &nstep);

		r->held[s] = 0;
		if (!st || (st->ms == 0 && r->step[s] + 1 >= nstep)) {
			r->edge[s] = 1;
			r->step[s] = 0;
			r->step_at[s] = now;
			r->lit_slot = -1;
			if (b->n_up == 0)
				r->used[s] = 0;
		}
	} else {
		return 0;
	}
	return present(r, now, w, maxw, timer_ms);
}

int mrun_tick(struct mrun *r, long long now,
	      struct gwrite *w, int maxw, int *timer_ms)
{
	return present(r, now, w, maxw, timer_ms);
}
