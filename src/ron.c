#include "ron.h"

#include <stdio.h>
#include <string.h>

static const char *skip_ws(const char *p)
{
	while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
		p++;
	return p;
}

static int field_int(const char *from, const char *key, int *out)
{
	const char *p = strstr(from, key);
	int v = 0;
	int any = 0;

	if (!p)
		return -1;
	p += strlen(key);
	p = skip_ws(p);
	if (*p == '-')
		return -1;
	while (*p >= '0' && *p <= '9') {
		any = 1;
		v = v * 10 + (*p - '0');
		if (v > 255)
			return -1;
		p++;
	}
	if (!any)
		return -1;
	*out = v;
	return 0;
}

int ron_parse(const char *text, int *r, int *g, int *b, int *bri)
{
	const char *p;
	const char *c;

	if (!text || !r || !g || !b || !bri)
		return -1;
	p = strstr(text, "brightness:");
	if (!p)
		return -1;
	p = skip_ws(p + strlen("brightness:"));
	if (strncmp(p, "Off", 3) == 0)
		*bri = 0;
	else if (strncmp(p, "Low", 3) == 0)
		*bri = 1;
	else if (strncmp(p, "Med", 3) == 0)
		*bri = 2;
	else if (strncmp(p, "High", 4) == 0)
		*bri = 3;
	else
		return -1;

	c = strstr(text, "colour1:");
	if (!c)
		c = strstr(text, "color1:");
	if (!c)
		return -1;
	if (field_int(c, "r:", r) || field_int(c, "g:", g) || field_int(c, "b:", b))
		return -1;
	return 0;
}

int ron_load(const char *path, int *r, int *g, int *b, int *bri)
{
	FILE *f;
	char buf[8192];
	size_t n;

	f = fopen(path, "r");
	if (!f)
		return -1;
	n = fread(buf, 1, sizeof buf - 1, f);
	fclose(f);
	if (n == 0)
		return -1;
	buf[n] = 0;
	return ron_parse(buf, r, g, b, bri);
}
