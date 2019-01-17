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

//bool ports[NPORT];//reverse map to check or look for an unassigned port fast but memory costly, needs to think more

int countSock;
int nextsid;

//============================helper functions declaration====================
/// sets value to socket struct
void setsockvalue(struct sock *socket, int sid, enum sockstate state, struct proc *owner, int lPort, int rPort,
                  void *chan, bool hasfullbuffer, char *buff, bool overWriteToDef);

/// returns socket @ port, NULL if no socket exists
struct sock *getsock(int port);

/// allocates a new socket @ port, NULL if any socket already exists @ that port.
/// increments count of current socket
struct sock *allocsock(int lport);

/// removes socket ,false if no socket exists or not called by owner
/// decrements count of current socket
bool removesock(struct sock *socket);

/// resets socket values to default
void resetsock(struct sock *socket);

/// returns a random free port
int getfreeport();

/// blocks the process
void blockprocess(struct proc *process);

/// releases associated process
void releaseprocess(struct proc *process);

/// returns and releases the lock
int retsockfunction(int code);
//============================================================================

void
sinit(void) {
	initlock(&stable.lock, "stable");
	countSock = 0;
	nextsid = 0;
}

/// new ServerSocket(port) // from server
int
listen(int lport) {
	DISCARD_INV_PORT(lport);

	acquire(&stable.lock);

	if (countSock == NSOCK) {
		return retsockfunction(E_FAIL);
	}
#ifdef SO_DEBUG
	if (countSock > NSOCK) {
		cprintf(">> countSock invalid: %d\n", countSock);
		return retsockfunction(E_FAIL);
	}
#endif
	struct sock *s;
	if ((s = allocsock(lport)) == NULL) {
		// trying to listen on opened port
		return retsockfunction(E_ACCESS_DENIED);
	}
	s->state = LISTENING;// set socket state to listening for server
	return retsockfunction(0);
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

	if (lport == INV_PORT) return retsockfunction(E_FAIL); // no free ports
	if (remote == NULL) return retsockfunction(E_NOTFOUND); // no socket assigned on remote port
	if (remote->state != LISTENING)
		return retsockfunction(E_WRONG_STATE); // if server is not ready to accept new connection or already connected

	if ((local = allocsock(lport)) == NULL) { // create a socket on free local port
		return retsockfunction(E_FAIL);// nSocket limits crossed
	}

	local->state = remote->state = CONNECTED;
	local->rPort = remote->lPort;
	remote->rPort = local->lPort;
	remote->hasfullbuffer = local->hasfullbuffer = false;

	release(&stable.lock);

	return lport;
}

/// socket.getos().write(data)
int
send(int lport, const char *data, int n) {
	DISCARD_INV_PORT(lport);

	acquire(&stable.lock);

	struct sock *local = getsock(lport);

	if (local == NULL) return retsockfunction(E_NOTFOUND);
	if (local->owner != myproc()) return retsockfunction(E_ACCESS_DENIED); // accessed from other process
	if (local->state != CONNECTED) return retsockfunction(E_WRONG_STATE);

	struct sock *remote = getsock(local->rPort);

	if (remote == NULL) return retsockfunction(E_NOTFOUND);
	if (remote->state != CONNECTED) return retsockfunction(E_WRONG_STATE);

	while (remote->hasfullbuffer) blockprocess(myproc()); // while or if not sure ???

	strncpy(remote->recvbuffer, data, n);
	remote->hasfullbuffer = true;
	releaseprocess(remote->owner);

	return retsockfunction(0);
}

/// socket.getis().read()
int
recv(int lport, char *data, int n) {
	DISCARD_INV_PORT(lport);

	acquire(&stable.lock);

	struct sock *local = getsock(lport);

	if (local == NULL) return retsockfunction(E_NOTFOUND);
	if (local->owner != myproc()) return retsockfunction(E_ACCESS_DENIED); // accessed from other process
	if (local->state != CONNECTED) return retsockfunction(E_WRONG_STATE);

	struct sock *remote = getsock(local->rPort);

	if (remote == NULL) return retsockfunction(E_NOTFOUND);
	if (remote->state != CONNECTED) return retsockfunction(E_WRONG_STATE);

	while (!local->hasfullbuffer) blockprocess(myproc()); // while or if not sure ???

	strncpy(data, local->recvbuffer, n);
	local->hasfullbuffer = false;
	releaseprocess(local->owner);

	return retsockfunction(0);
}

/// socket.close()
int
disconnect(int lport) {
	DISCARD_INV_PORT(lport);

	acquire(&stable.lock);

	struct sock *local = getsock(lport);

	if (local == NULL) return retsockfunction(E_NOTFOUND);
	if (local->owner != myproc()) return retsockfunction(E_ACCESS_DENIED); // accessed from other process
	removesock(local);

	struct sock *remote = getsock(local->rPort);

	if (remote == NULL) return retsockfunction(E_NOTFOUND);
	if (remote->state != CONNECTED) return retsockfunction(E_WRONG_STATE);
	remote->state = CLOSED;

#ifdef SO_DEBUG
	if (lport != remote->rPort)
		cprintf(">> Fatal Bug: local:%d remotes_remote:%d\n", lport, remote->rPort);
#endif
	return retsockfunction(0);
}


//============================helper functions definition====================

struct sock *
getsock(int port) {
	if (port >= 0 && port < NPORT) {
		struct sock *s;
		for (s = stable.sock; s < &stable.sock[NSOCK]; ++s) {
			if (s->state != CLOSED && s->lPort == port) {
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
//	socket->hasfullbuffer = SOCK_HASDATA_DEF;
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
			setsockvalue(s, nextsid, OPENED, myproc(), lport, SOCK_RPORT_DEF, SOCK_CHAN_DEF, false, "", true);

			countSock++;
			nextsid++;
			return s;
		}
	}
	return NULL;
}

void
setsockvalue(struct sock *socket, int sid, enum sockstate state, struct proc *owner, int lPort, int rPort,
             void *chan, bool hasfullbuffer, char *buff, bool overWriteToDef) {
	if (!socket) return;
	if (overWriteToDef) {
		socket->owner = owner;
		socket->state = state;
		socket->lPort = lPort;
		socket->rPort = rPort;
		socket->chan = chan;
		socket->hasfullbuffer = hasfullbuffer;
		socket->sid = sid;
		strncpy(socket->recvbuffer, buff, 1);
	} else {
		if (owner != SOCK_OWNER_DEF) socket->owner = owner;
		if (state != SOCK_STATE_DEF) socket->state = state;
		if (lPort != SOCK_LPORT_DEF) socket->lPort = lPort;
		if (rPort != SOCK_RPORT_DEF) socket->rPort = rPort;
		if (chan != SOCK_CHAN_DEF) socket->chan = chan;
		if (hasfullbuffer != SOCK_HASDATA_DEF) socket->hasfullbuffer = hasfullbuffer;
		if (sid != SOCK_SID_DEF) socket->sid = sid;
		if (strncmp(buff, SOCK_BUFFER_DEF, 1) != 0) strncpy(socket->recvbuffer, buff, 1);
	}
}

/// basically returns the first free port from the last
int
getfreeport() {
	for (int port = NPORT; port; --port) {
		if (getsock(port) == NULL) return port;
	}
	return INV_PORT;
}

bool
removesock(struct sock *socket) {
	if (socket == NULL) return false;
	resetsock(socket);
	--countSock;
	return true;
}

int
retsockfunction(int code) {
	release(&stable.lock);
	return code;
}

//============================================================================