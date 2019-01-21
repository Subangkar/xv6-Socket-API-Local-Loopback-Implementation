#include "types.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sock.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "signalcodes.h"




struct {
	struct spinlock lock;
	struct sock sock[NSOCK];
} stable;


int countSock;
int nextsid;

//============================helper functions declaration====================
/// sets value to socket struct
void setsockvalue(struct sock *socket, int sid, enum sockstate state, struct proc *owner, int lPort, int rPort,
                  bool hasfullbuffer, char *buff, bool overWriteToDef);

/// returns socket @ port, NULL if no socket exists
struct sock *getsock(int port);

/// allocates a new socket @ port, NULL if any socket already exists @ that port.
/// increments count of current socket
struct sock *allocsock(int lport);

/// removes socket ,false if no socket exists or not called by owner
/// decrements count of current socket and also releases locks
bool removesock(struct sock *socket);

/// resets socket values to default
void resetsock(struct sock *socket);

/// returns a random free port
int getfreeport();

/// returns and releases the lock
int retsockfunc(int code);

/// returns the first socket in the table associated with the process, NULL if no such found
struct sock *getprocsock(struct proc *process);
//============================================================================

void
sinit(void) {
	initlock(&stable.lock, "stable");
	countSock = 0;
	nextsid = 0;
}

/// new ServerSocket(port).accept() // from server
int
listen(int lport) {
	DISCARD_INV_PORT(lport);

	acquire(&stable.lock);

	if (countSock == NSOCK) {
		return retsockfunc(E_FAIL);
	}

	struct sock *s;
	if ((s = allocsock(lport)) == NULL) {
		// trying to listen on opened port
		return retsockfunc(E_ACCESS_DENIED);
	}
	s->state = LISTENING;// set socket state to listening for server
	sleep(s, &stable.lock);// wait until any client connects

	return retsockfunc(0);
}

/// new Socket(hostname,port) // from client
int
connect(int rport, const char *host) {
	DISCARD_INV_PORT(rport);

	if (strncmp(host, "localhost", (uint) strlen(host)) != 0
	    && strncmp(host, "127.0.0.1", (uint) strlen(host)) != 0) {
		// Allow connection to "localhost" or "127.0.0.1" host only
		return E_INVALID_ARG;
	}

	acquire(&stable.lock);

	struct sock *local;
	struct sock *remote = getsock(rport);
	int lport = getfreeport();

	if (lport == INV_PORT) return retsockfunc(E_FAIL); // no free ports
	if (remote == NULL) return retsockfunc(E_NOTFOUND); // no socket assigned on remote port
	if (remote->state != LISTENING)
		return retsockfunc(E_WRONG_STATE); // if server is not ready to accept new connection or already connected

	if ((local = allocsock(lport)) == NULL) { // create a socket on free local port
		return retsockfunc(E_FAIL);// nSocket limits crossed
	}

	local->state = remote->state = CONNECTED;
	local->rPort = remote->lPort;
	remote->rPort = local->lPort;
	remote->hasfullbuffer = local->hasfullbuffer = false;
	wakeup(remote);// wakeup the remote listening server

	return retsockfunc(lport);
}

/// socket.getos().write(data)
int
send(int lport, const char *data, int n) {
	DISCARD_INV_PORT(lport);

	acquire(&stable.lock);

	struct sock *local = getsock(lport);

	if (local == NULL) return retsockfunc(E_NOTFOUND);
	if (local->owner != myproc()) return retsockfunc(E_ACCESS_DENIED); // accessed from other process
	if (local->state != CONNECTED) return retsockfunc(E_WRONG_STATE);

	struct sock *remote = getsock(local->rPort);

	if (remote == NULL) return retsockfunc(E_NOTFOUND);
	if (remote->state != CONNECTED) return retsockfunc(E_WRONG_STATE);

	if (remote->hasfullbuffer) sleep(remote, &stable.lock);// while or if not sure ???

	if (getsock(local->rPort) == NULL) {// woke up due to deletion of remote
		removesock(local);
		return retsockfunc(E_CONNECTION_RESET_BY_REMOTE);
	}

	strncpy(remote->recvbuffer, data, n);
	remote->hasfullbuffer = true;
	wakeup(local);

	return retsockfunc(0);
}

/// socket.getis().read()
int
recv(int lport, char *data, int n) {
	DISCARD_INV_PORT(lport);

	acquire(&stable.lock);

	struct sock *local = getsock(lport);

	if (local == NULL) return retsockfunc(E_NOTFOUND);
	if (local->owner != myproc()) return retsockfunc(E_ACCESS_DENIED); // accessed from other process
	if (local->state != CONNECTED) return retsockfunc(E_WRONG_STATE);

	struct sock *remote = getsock(local->rPort);

	if (remote == NULL) return retsockfunc(E_NOTFOUND);// last sent data can also be blocked here
	if (remote->state != CONNECTED) return retsockfunc(E_WRONG_STATE);

	if (!local->hasfullbuffer) sleep(remote, &stable.lock);// while or if not sure ???

	strncpy(data, local->recvbuffer, n);
	*local->recvbuffer = NULL;// empty the buffer after reading
	local->hasfullbuffer = false;
	wakeup(local);

	// after reading last unread buffer if connection is reset then close
	if (getsock(local->rPort) == NULL) {// woke up due to deletion of remote
		removesock(local);
		return retsockfunc(E_CONNECTION_RESET_BY_REMOTE);
	}

	return retsockfunc(0);
}

/// socket.close()
int
disconnect(int lport) {
	DISCARD_INV_PORT(lport);

	acquire(&stable.lock);

	struct sock *local = getsock(lport);

	if (local == NULL) return retsockfunc(E_NOTFOUND);
	if (local->owner != myproc()) return retsockfunc(E_ACCESS_DENIED); // accessed from other process
	removesock(local);

	return retsockfunc(0);
}


//============================helper functions definition====================

struct sock *
getsock(int port) {
	if (port >= 0 && port < NPORT) {
		for (struct sock *s = stable.sock; s < &stable.sock[NSOCK]; ++s) {
			if (s->state != CLOSED && s->lPort == port) {
				return s;
			}
		}
	}
	return NULL;
}

void
resetsock(struct sock *socket) {
	setsockvalue(socket, SOCK_SID_DEF, SOCK_STATE_DEF, SOCK_OWNER_DEF, SOCK_LPORT_DEF, SOCK_RPORT_DEF,
	             SOCK_HASDATA_DEF, SOCK_BUFFER_DEF, true);
}

struct sock *
allocsock(int lport) {
	if (lport < 0 || lport >= NPORT || getsock(lport)) return NULL;
	for (struct sock *s = stable.sock; s < &stable.sock[NSOCK]; ++s) {
		if (s->state == CLOSED) {
			setsockvalue(s, nextsid, OPENED, myproc(), lport, SOCK_RPORT_DEF, false, "", true);

			countSock++;
			nextsid++;
#ifdef SO_DEBUG
			cprintf(">> Adding socket %d, #Socket:%d\n", s->sid, countSock);
#endif
			return s;
		}
	}
	return NULL;
}

void
setsockvalue(struct sock *socket, int sid, enum sockstate state, struct proc *owner, int lPort, int rPort,
             bool hasfullbuffer, char *buff, bool overWriteToDef) {
	if (!socket) return;
	if (overWriteToDef) {
		socket->owner = owner;
		socket->state = state;
		socket->lPort = lPort;
		socket->rPort = rPort;
		socket->hasfullbuffer = hasfullbuffer;
		socket->sid = sid;
		strncpy(socket->recvbuffer, buff, 1);
	} else {
		if (owner != SOCK_OWNER_DEF) socket->owner = owner;
		if (state != SOCK_STATE_DEF) socket->state = state;
		if (lPort != SOCK_LPORT_DEF) socket->lPort = lPort;
		if (rPort != SOCK_RPORT_DEF) socket->rPort = rPort;
		if (hasfullbuffer != SOCK_HASDATA_DEF) socket->hasfullbuffer = hasfullbuffer;
		if (sid != SOCK_SID_DEF) socket->sid = sid;
		if (strncmp(buff, SOCK_BUFFER_DEF, 1) != 0) strncpy(socket->recvbuffer, buff, 1);
	}
}

/// basically returns the first free port from the last
int
getfreeport() {
	for (int port = NPORT - 1; port; --port) {
		if (getsock(port) == NULL) return port;
	}
	return INV_PORT;
}

bool
removesock(struct sock *socket) {
	if (socket == NULL) return false;
#ifdef SO_DEBUG
	cprintf(">> Removing socket %d, #Socket:%d\n", socket->sid, countSock - 1);
#endif
	wakeup(socket);// wakeup all processes sleeping on this lock
	resetsock(socket);
	--countSock;
	return true;
}

int
retsockfunc(int code) {
	release(&stable.lock);
	return code;
}

struct sock *
getprocsock(struct proc *process) {
	for (struct sock *s = stable.sock; s < &stable.sock[NSOCK]; ++s) {
		if (s->state != CLOSED && s->owner == process) {
			return s;
		}
	}
	return NULL;
}

//============================================================================


//===========================Additional functions=============================

/// close all socket owned by this process
void
closeprocsocks(struct proc *process) {
	if (process == NULL) return;
	struct sock *s;
	acquire(&stable.lock);
	while ((s = getprocsock(process)) != NULL) {
		removesock(s);
//		disconnect(s->lPort);// introduces recursive lock
	}
	release(&stable.lock);
}

//============================================================================