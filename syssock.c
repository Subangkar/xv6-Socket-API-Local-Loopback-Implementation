#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "signalcodes.h"

int
sys_listen(void) {
	int port = 0;

	if (argint(0, &port) < 0)
		return E_INVALID_ARG;
#ifdef SO_DEBUG
	cprintf(">> %d\n", port);
#endif
	return listen(port);
}

int
sys_connect(void) {
	int port = 0;
	char *host = 0;

	if (argint(0, &port) < 0 || argstr(1, &host) < 0)
		return E_INVALID_ARG;
#ifdef SO_DEBUG
	cprintf(">> %d %s\n", port, host);
#endif
	return connect(port, host);
}

int
sys_send(void) {
	int port = 0;
	char *buf = 0;
	int n = 0;

	if (argint(0, &port) < 0 || argstr(1, &buf) < 0 || argint(0, &n) < 0)
		return E_INVALID_ARG;

#ifdef SO_DEBUG
	cprintf(">> %d %s %d\n", port, buf, n);
#endif

	return send(port, buf, n);
}

int
sys_recv(void) {
	int port = 0;
	char *buf = 0;
	int n = 0;

	if (argint(0, &port) < 0 || argptr(1, &buf, sizeof(char *)) < 0 || argint(0, &n) < 0)
		return E_INVALID_ARG;

#ifdef SO_DEBUG
	cprintf(">> %d %s %d\n", port, buf, n);
#endif
	return recv(port, buf, n);
}

int
sys_disconnect(void) {
	int port = 0;

	if (argint(0, &port) < 0)
		return E_INVALID_ARG;
#ifdef SO_DEBUG
	cprintf(">> %d\n", port);
#endif

	return disconnect(port);
}
