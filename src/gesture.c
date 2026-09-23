#include "gesture.h"

static int lerp(int a, int b, long long num, long long den)
{
	if (den <= 0)
		return b;
	if (num < 0)
		num = 0;
	if (num > den)
		num = den;
	return (int)(a + (long long)(b - a) * num / den);
}

static int lerp_round(int a, int b, long long num, long long den)
{
	long long n;

	if (den <= 0)
		return b;
	if (num < 0)
		num = 0;
	if (num > den)
		num = den;
	n = (long long)(b - a) * num;
	if (n >= 0)
		n += den / 2;
	else
		n -= den / 2;
	return (int)(a + n / den);
}

static int erase_active(const struct gstate *s, long long now)
{
	if (s->erase_down)
		return 1;
	if (s->fade_at && now < s->fade_at + 2000)
		return 1;
	return 0;
}

void g_init(struct gstate *s, int r, int g, int b, int bri)
{
	s->nr = r;
	s->ng = g;
	s->nb = b;
	s->nbri = bri;
	s->meta_n = 0;
	s->meta_since = 0;
	s->erase_n = 0;
	s->erase_down = 0;
	s->erase_since = 0;
	s->fade_at = 0;
	s->enter_on = 0;
	s->enter_since = 0;
	s->lit = -1;
	s->lit_since = 0;
	s->lit_bri = -1;
}

void g_set_normal(struct gstate *s, int r, int g, int b, int bri)
{
	s->nr = r;
	s->ng = g;
	s->nb = b;
	s->nbri = bri;
}

int g_owner(const struct gstate *s, long long now)
{
	long long best = -1;
	int id = G_NONE;

	if (s->meta_n > 0 && s->meta_since >= best) {
		best = s->meta_since;
		id = G_META;
	}
	if (erase_active(s, now) && s->erase_since >= best) {
		best = s->erase_since;
		id = G_ERASE;
	}
	if (s->enter_on && s->enter_since >= best)
		id = G_ENTER;
	return id;
}

long long g_meta_since(const struct gstate *s)
{
	return s->meta_since;
}

long long g_erase_since(const struct gstate *s)
{
	return s->erase_since;
}

static void hold_color(const struct gstate *s, long long now,
		       int *r, int *g, int *b, int *bri)
{
	long long e = now - s->erase_since;

	if (e < 0)
		e = 0;
	if (e > 5000)
		e = 5000;
	*r = lerp(s->base_r, 255, e, 5000);
	*g = lerp(s->base_g, 0, e, 5000);
	*b = lerp(s->base_b, 0, e, 5000);
	*bri = lerp_round(s->base_bri, 3, e, 5000);
}

static int push_mode(struct gwrite *w, int n, int max,
		     int mode, int r, int g, int b)
{
	if (n >= max)
		return n;
	w[n].kind = GW_MODE;
	w[n].mode = mode;
	w[n].r = r;
	w[n].g = g;
	w[n].b = b;
	w[n].speed = 1;
	w[n].brightness = -1;
	return n + 1;
}

static int push_bri(struct gwrite *w, int n, int max, int bri)
{
	if (n >= max)
		return n;
	w[n].kind = GW_BRIGHT;
	w[n].mode = 0;
	w[n].r = 0;
	w[n].g = 0;
	w[n].b = 0;
	w[n].speed = 0;
	w[n].brightness = bri;
	return n + 1;
}

static int present(struct gstate *s, long long now,
		   struct gwrite *w, int maxw, int *timer_ms)
{
	int n = 0;
	int owner;
	int r, g, b, bri;
	long long elapsed;

	*timer_ms = 0;
	owner = g_owner(s, now);
	if (owner == G_ENTER) {
		n = push_bri(w, n, maxw, 0);
		n = push_bri(w, n, maxw, s->nbri);
		n = push_bri(w, n, maxw, 0);
		n = push_bri(w, n, maxw, s->nbri);
		s->lit_bri = s->nbri;
		s->enter_on = 0;
		owner = g_owner(s, now);
	}
	if (owner == G_NONE) {
		if (s->lit != G_NONE) {
			n = push_mode(w, n, maxw, 0, s->nr, s->ng, s->nb);
			n = push_bri(w, n, maxw, s->nbri);
			s->lit_bri = s->nbri;
			s->lit = G_NONE;
			s->lit_since = 0;
		}
		return n;
	}
	if (owner == G_META) {
		if (s->lit != G_META || s->lit_since != s->meta_since) {
			n = push_mode(w, n, maxw, 1, s->nr, s->ng, s->nb);
			if (s->lit_bri != s->nbri) {
				n = push_bri(w, n, maxw, s->nbri);
				s->lit_bri = s->nbri;
			}
			s->lit = G_META;
			s->lit_since = s->meta_since;
		}
		return n;
	}

	/* erase owns the LED */
	if (s->erase_down) {
		elapsed = now - s->erase_since;
		if (elapsed < 0)
			elapsed = 0;
		if (elapsed >= 5000) {
			r = 255;
			g = 0;
			b = 0;
			bri = 3;
			*timer_ms = 0;
		} else {
			hold_color(s, now, &r, &g, &b, &bri);
			*timer_ms = 50;
		}
	} else {
		elapsed = now - s->fade_at;
		if (elapsed < 0)
			elapsed = 0;
		if (elapsed >= 2000) {
			r = s->fade_to_r;
			g = s->fade_to_g;
			b = s->fade_to_b;
			bri = s->fade_to_bri;
			*timer_ms = 0;
		} else {
			r = lerp(s->fade_r, s->fade_to_r, elapsed, 2000);
			g = lerp(s->fade_g, s->fade_to_g, elapsed, 2000);
			b = lerp(s->fade_b, s->fade_to_b, elapsed, 2000);
			bri = lerp_round(s->fade_bri, s->fade_to_bri, elapsed, 2000);
			*timer_ms = 50;
		}
	}
	n = push_mode(w, n, maxw, 0, r, g, b);
	if (bri != s->lit_bri) {
		n = push_bri(w, n, maxw, bri);
		s->lit_bri = bri;
	}
	s->lit = G_ERASE;
	s->lit_since = s->erase_since;
	return n;
}

static void begin_erase(struct gstate *s, long long now)
{
	s->erase_since = now;
	s->erase_down = 1;
	s->fade_at = 0;
	s->base_r = s->nr;
	s->base_g = s->ng;
	s->base_b = s->nb;
	s->base_bri = s->nbri;
}

static void release_erase(struct gstate *s, long long now)
{
	s->erase_down = 0;
	s->fade_at = now;
	hold_color(s, now, &s->fade_r, &s->fade_g, &s->fade_b, &s->fade_bri);
	s->fade_to_r = s->nr;
	s->fade_to_g = s->ng;
	s->fade_to_b = s->nb;
	s->fade_to_bri = s->nbri;
}

int g_key(struct gstate *s, int code, int value, long long now,
	  struct gwrite *w, int maxw, int *timer_ms)
{
	int watched = 0;

	*timer_ms = -1;
	if (value == 2)
		return 0;
	if (value != 0 && value != 1)
		return 0;

	if (code == DB_KEY_LEFTMETA || code == DB_KEY_RIGHTMETA) {
		watched = 1;
		if (value == 1) {
			if (s->meta_n == 0)
				s->meta_since = now;
			s->meta_n++;
		} else if (s->meta_n > 0) {
			s->meta_n--;
		}
	} else if (code == DB_KEY_BACKSPACE || code == DB_KEY_DELETE) {
		watched = 1;
		if (value == 1) {
			if (s->erase_n == 0)
				begin_erase(s, now);
			s->erase_n++;
		} else if (s->erase_n > 0) {
			s->erase_n--;
			if (s->erase_n == 0 && s->erase_down)
				release_erase(s, now);
		}
	} else if (code == DB_KEY_ENTER) {
		watched = 1;
		if (value == 1) {
			s->enter_on = 1;
			s->enter_since = now;
		}
	}

	if (!watched)
		return 0;
	return present(s, now, w, maxw, timer_ms);
}

int g_tick(struct gstate *s, long long now,
	   struct gwrite *w, int maxw, int *timer_ms)
{
	return present(s, now, w, maxw, timer_ms);
}
