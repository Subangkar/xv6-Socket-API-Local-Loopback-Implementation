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

//
// Should it have locking?
//

struct {
	struct spinlock lock;
	struct sock sock[NSOCK];
} stable;

int countSock;
int nextsid;

//==================helper functions declaration==========
/// sets value to socket struct
void setsockvalue(struct sock *socket, int sid, enum sockstate state, struct proc *owner, int lPort, int rPort,
                  void *chan, bool hasUnreadData, char *buff, bool overWriteToDef);

/// returns socket @ port, NULL if no socket exists
struct sock *getsock(int port);

/// allocates a new socket @ port, NULL if any socket already exists @ that port
struct sock *allocsock(int lport);

/// frees socket by clearing buffer, false if no socket exists or not called by owner
bool freesock(struct sock *socket);

/// resets socket values to default
void resetsock(struct sock *socket);
//========================================================

void
sinit(void) {
	initlock(&stable.lock, "stable");
	countSock = 0;
	nextsid = 0;
}

/// new ServerSocket(port)
int
listen(int lport) {
	DISCARD_INV_PORT(lport);

	if (countSock == NSOCK) {
		return E_FAIL;
	}
#ifdef SO_DEBUG
	if (countSock > NSOCK) {
		cprintf(">> countSock invalid: %d\n", countSock);
		return E_FAIL;
	}
#endif
	acquire(&stable.lock);
	if (allocsock(lport) == NULL) {
		// trying to listen on opened port
		release(&stable.lock);
		return E_ACCESS_DENIED;
	}
	release(&stable.lock);
	return 0;
}

/// new Socket(hostname,port)
int
connect(int rport, const char *host) {
	DISCARD_INV_PORT(rport);

	//
	// Allow connection to "localhost" or "127.0.0.1" host only
	//
	if (strncmp(host, "localhost", (uint) strlen(host)) != 0
	    && strncmp(host, "127.0.0.1", (uint) strlen(host)) != 0) {
		return E_INVALID_ARG;
	}

	return 0;
}

int
send(int lport, const char *data, int n) {
	DISCARD_INV_PORT(lport);

	return 0;
}


int
recv(int lport, char *data, int n) {
	DISCARD_INV_PORT(lport);
	
	return 0;
}

int
disconnect(int lport) {
	DISCARD_INV_PORT(lport);

	return 0;
}


struct sock *
getsock(int port) {
	if (port >= 0 && port < NPORT) {
		struct sock *s;
		for (s = stable.sock; s < &stable.sock[NSOCK]; ++s) {
			if (s->lPort == port && s->state != CLOSED) {
				return s;
			}
		}
	}
	return NULL;
}

void
resetsock(struct sock *socket) {
//	if (socket == NULL) return;
//	socket->owner = SOCK_OWNER_DEF;
//	socket->state = SOCK_STATE_DEF;
//	socket->lPort = SOCK_LPORT_DEF;
//	socket->rPort = SOCK_RPORT_DEF;
//	socket->chan = SOCK_CHAN_DEF;
//	socket->hasUnreadData = SOCK_HASDATA_DEF;
//	socket->sid = SOCK_SID_DEF;
//	strncpy(socket->recvbuffer, SOCK_BUFFER_DEF, 1);
	setsockvalue(socket, SOCK_SID_DEF, SOCK_STATE_DEF, SOCK_OWNER_DEF, SOCK_LPORT_DEF, SOCK_RPORT_DEF, SOCK_CHAN_DEF,
	             SOCK_HASDATA_DEF, SOCK_BUFFER_DEF, true);
}

struct sock *
allocsock(int lport) {
	if (lport < 0 || lport >= NPORT || getsock(lport)) return NULL;
	struct sock *s;
	for (s = stable.sock; s < &stable.sock[NSOCK]; ++s) {
		if (s->state == CLOSED) {
			setsockvalue(s, nextsid, LISTENING, myproc(), lport, SOCK_RPORT_DEF, SOCK_CHAN_DEF, false, "", true);

			countSock++;
			nextsid++;
			return s;
		}
	}
	return NULL;
}

void
setsockvalue(struct sock *socket, int sid, enum sockstate state, struct proc *owner, int lPort, int rPort,
             void *chan, bool hasUnreadData, char *buff, bool overWriteToDef) {
	if (!socket) return;
	if (overWriteToDef) {
		socket->owner = owner;
		socket->state = state;
		socket->lPort = lPort;
		socket->rPort = rPort;
		socket->chan = chan;
		socket->hasUnreadData = hasUnreadData;
		socket->sid = sid;
		strncpy(socket->recvbuffer, buff, 1);
	} else {
		if (owner != SOCK_OWNER_DEF) socket->owner = owner;
		if (state != SOCK_STATE_DEF) socket->state = state;
		if (lPort != SOCK_LPORT_DEF) socket->lPort = lPort;
		if (rPort != SOCK_RPORT_DEF) socket->rPort = rPort;
		if (chan != SOCK_CHAN_DEF) socket->chan = chan;
		if (hasUnreadData != SOCK_HASDATA_DEF) socket->hasUnreadData = hasUnreadData;
		if (sid != SOCK_SID_DEF) socket->sid = sid;
		if (strncmp(buff, SOCK_BUFFER_DEF, 1) != 0) strncpy(socket->recvbuffer, buff, 1);
	}
}

