#ifndef DAEBOARD_RON_H
#define DAEBOARD_RON_H

/* 0 on success. brightness is 0..3 from Off/Low/Med/High. */
int ron_parse(const char *text, int *r, int *g, int *b, int *bri);
int ron_load(const char *path, int *r, int *g, int *b, int *bri);

#endif
