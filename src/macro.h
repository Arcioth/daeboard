#ifndef DAEBOARD_MACRO_H
#define DAEBOARD_MACRO_H

#include "gesture.h"

#define M_STEPS 32
#define M_BINDS 24

enum {
	M_COLOR = 1,
	M_BRIGHT = 2,
	M_BREATHE = 3,
	M_FADE = 4,
	M_REST = 5
};

struct mstep {
	int op;
	int r, g, b, bri;
	int ms;
};

struct mbind {
	int code;
	int n_down, n_up;
	int has_ctron;
	char name[24];
	struct mstep down[M_STEPS];
	struct mstep up[M_STEPS];
};

struct mtable {
	struct mbind b[M_BINDS];
	int n;
	int err_line;
};

struct mrun {
	struct mtable tab;
	int nr, ng, nb, nbri;
	/* playback */
	int code[8];
	int used[8];
	int held[8];
	int edge[8];
	int step[8];
	long long step_at[8];
	long long since[8];
	int fade_r[8], fade_g[8], fade_b[8], fade_bri[8];
	int lit_slot;
	int lit_edge;
	int lit_step;
	int lit_ms_bucket;
	char pending_fire[24];
};

int macro_compile(const char *text, struct mtable *t);
void macro_builtin(struct mtable *t);
/* 0 ok, -1 compile error (err_line set), -2 unreadable. */
int macro_load(const char *path, struct mtable *t);

void mrun_init(struct mrun *r, int red, int grn, int blu, int bri);
void mrun_set_normal(struct mrun *r, int red, int grn, int blu, int bri);
void mrun_set_table(struct mrun *r, const struct mtable *t);
int mrun_idle(const struct mrun *r);

int mrun_key(struct mrun *r, int code, int value, long long now,
	     struct gwrite *w, int maxw, int *timer_ms);
int mrun_tick(struct mrun *r, long long now,
	      struct gwrite *w, int maxw, int *timer_ms);

#endif
