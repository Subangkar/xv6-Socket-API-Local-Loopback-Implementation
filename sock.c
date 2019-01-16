#include "types.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sock.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"

//
// TODO: Create a structure to maintain a list of sockets
// Should it have locking?
//

void
sinit(void)
{
  //
  // TODO: Write any initialization code for socket API
  // initialization.
  //
}

int
listen(int lport) {

  //
  // TODO: Put the actual implementation of listen here.
  //

  return 0;
}

int
connect(int rport, const char* host) {
  //
  // TODO: Put the actual implementation of connect here.
  //

  return 0;
}

int
send(int lport, const char* data, int n) {
  //
  // TODO: Put the actual implementation of send here.
  //

  return 0;
}


int
recv(int lport, char* data, int n) {
  //
  // TODO: Put the actual implementation of recv here.
  //

  return 0;
}

int
disconnect(int lport) {
  //
  // TODO: Put the actual implementation of disconnect here.
  //

  return 0;
}
