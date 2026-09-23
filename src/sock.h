#ifndef DAEBOARD_SOCK_H
#define DAEBOARD_SOCK_H

#define SOCK_CLIENTS 4
#define SOCK_PATH "/run/daeboard/daeboard.sock"

int sock_listen(void);
/* 0 ok, reply filled. -1 drop the client. */
int sock_command(const char *line, char *reply, int reply_max,
		 int *r, int *g, int *b, int *bri, int *quit);

#endif
