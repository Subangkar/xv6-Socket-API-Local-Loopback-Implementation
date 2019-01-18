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

/// sends close signal to remote socket on local's one close like TCP
bool sendFINsignal(struct sock *socket);

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
#ifdef SO_DEBUG
	cprintf(">> Server  listening on %d\n", lport);

	if (countSock > NSOCK) {
		cprintf(">> countSock invalid: %d\n", countSock);
		return retsockfunc(E_FAIL);
	}
#endif
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

#ifdef SO_DEBUG
	cprintf(">> %d client to be assigned port %d: \n", lport);
#endif

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

#ifdef SO_FUNC_DEBUG
	cprintf(">> %d.send() obtained remote %d\n", local->lPort, remote->lPort);
#endif

	while (remote->hasfullbuffer) sleep(remote, &stable.lock);// while or if not sure ???

	strncpy(remote->recvbuffer, data, n);
	remote->hasfullbuffer = true;
	wakeup(local);

#ifdef SO_DEBUG
	cprintf(">> %d sent msg to %d: %s\n", local->lPort, remote->lPort, data);
	if (lport != remote->rPort)
		cprintf(">> %d Fatal Bug: local:%d remotes_remote:%d\n", lport, lport, remote->rPort);
#endif

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

#ifdef SO_FUNC_DEBUG
	cprintf(">> %d.recv() \n", lport);
#endif

	struct sock *remote = getsock(local->rPort);

//	if (remote == NULL && !strncmp(data, SOCK_MSG_FIN, (uint) strlen(SOCK_MSG_FIN)))
//		removesock(local);// check whether socket closed by sending FIN pkt
	if (remote == NULL) return retsockfunc(E_NOTFOUND);// FIN is blocked here
	if (remote->state != CONNECTED) return retsockfunc(E_WRONG_STATE);

#ifdef SO_FUNC_DEBUG
	cprintf(">> %d.recv() has remote %d\n", local->lPort, remote->lPort);
#endif

	while (!local->hasfullbuffer) sleep(remote, &stable.lock);// while or if not sure ???

	strncpy(data, local->recvbuffer, n);
	local->hasfullbuffer = false;
	wakeup(local);

#ifdef SO_DEBUG
	cprintf(">> %d received msg from %d: %s\n", local->lPort, remote->lPort, data);
	if (lport != remote->rPort)
		cprintf(">> Fatal Bug: local:%d remotes_remote:%d\n", lport, remote->rPort);
#endif

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

	struct sock *remote = getsock(local->rPort);

	if (remote == NULL) return retsockfunc(E_NOTFOUND);
	if (remote->state != CONNECTED) return retsockfunc(E_WRONG_STATE);
	sendFINsignal(remote);

#ifdef SO_DEBUG
	if (lport != remote->rPort)
		cprintf(">> Fatal Bug: local:%d remotes_remote:%d\n", lport, remote->rPort);
#endif
	return retsockfunc(0);
}


//============================helper functions definition====================

struct sock *
getsock(int port) {
	if (port >= 0 && port < NPORT) {
		struct sock *s;
		for (s = stable.sock; s < &stable.sock[NSOCK]; ++s) {
			if (s->state != CLOSED && s->lPort == port) {
//#ifdef SO_DEBUG
//				cprintf(">> found ",s->lPort);
//#endif
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
	setsockvalue(socket, SOCK_SID_DEF, SOCK_STATE_DEF, SOCK_OWNER_DEF, SOCK_LPORT_DEF, SOCK_RPORT_DEF,
	             SOCK_HASDATA_DEF, SOCK_BUFFER_DEF, true);
}

struct sock *
allocsock(int lport) {
	if (lport < 0 || lport >= NPORT || getsock(lport)) return NULL;
	struct sock *s;
	for (s = stable.sock; s < &stable.sock[NSOCK]; ++s) {
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

bool
sendFINsignal(struct sock *socket) {
	if (socket == NULL) return false;
	if (socket->state != CLOSED) {
		strncpy(socket->recvbuffer, SOCK_MSG_FIN, strlen(SOCK_MSG_FIN));
		socket->hasfullbuffer = true;
		return true;
	}
	return false;
}

struct sock *
getprocsock(struct proc *process) {
	struct sock *s;
	for (s = stable.sock; s < &stable.sock[NSOCK]; ++s) {
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
//		disconnect(s->lPort);
	}
	release(&stable.lock);
}

//============================================================================