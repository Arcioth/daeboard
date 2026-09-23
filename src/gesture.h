#ifndef DAEBOARD_GESTURE_H
#define DAEBOARD_GESTURE_H

/* Linux evdev codes for the keys this daemon watches. */
enum {
	DB_KEY_BACKSPACE = 14,
	DB_KEY_ENTER = 28,
	DB_KEY_DELETE = 111,
	DB_KEY_LEFTMETA = 125,
	DB_KEY_RIGHTMETA = 126
};

enum {
	G_NONE = 0,
	G_META = 1,
	G_ENTER = 2,
	G_ERASE = 3
};

enum {
	GW_MODE = 1,
	GW_BRIGHT = 2
};

struct gwrite {
	int kind;
	int mode;
	int r, g, b, speed;
	int brightness;
};

struct gstate {
	int nr, ng, nb, nbri;
	int meta_n;
	long long meta_since;
	int erase_n;
	int erase_down;
	long long erase_since;
	int base_r, base_g, base_b, base_bri;
	long long fade_at;
	int fade_r, fade_g, fade_b, fade_bri;
	int fade_to_r, fade_to_g, fade_to_b, fade_to_bri;
	int enter_on;
	long long enter_since;
	int lit;
	long long lit_since;
	int lit_bri;
};

void g_init(struct gstate *s, int r, int g, int b, int bri);
void g_set_normal(struct gstate *s, int r, int g, int b, int bri);
int g_owner(const struct gstate *s, long long now);
long long g_meta_since(const struct gstate *s);
long long g_erase_since(const struct gstate *s);

/* timer_ms is 0 (disarm), 50, or -1 (this event changed nothing). */
int g_key(struct gstate *s, int code, int value, long long now,
	  struct gwrite *w, int maxw, int *timer_ms);
int g_tick(struct gstate *s, long long now,
	   struct gwrite *w, int maxw, int *timer_ms);

#endif
