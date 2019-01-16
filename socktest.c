#include "types.h"
#include "user.h"

int serverPort = 10;

void clientProc() {
  int clientPort;
  char buf[128];
  char host[16] = "localhost";

  // sleep for 100 clock ticks to ensure that the server process starts first.
  sleep(100);

  printf(1, "Client>> Attempting to connect to port %d, host %s ...\n", serverPort, host);
  clientPort = connect(serverPort, host);
  sleep(20);
  printf(1, "Client>> connect() returned %d\n", clientPort);

  while (1) {
    printf(1, "Client>> Enter text to send to server: ");
    gets(buf, sizeof(buf));
    buf[strlen(buf) - 1] = '\0'; // Eliminating the '\n'
    send(clientPort, buf, strlen(buf) + 1);

    if (0 == strcmp(buf, "exit")) {
      printf(1, "Client exiting...\n");
      disconnect(clientPort);
      break;
    }

    sleep(100 + uptime() % 100);

    recv(clientPort, buf, sizeof(buf));
    printf(1, "Client>> Received: \"%s\"\n", buf);
  }
}

void serverProc() {
  int status;
  char buf[128];

  printf(1, "Server>> Starting to listen at port %d ...\n", serverPort);
  status = listen(serverPort);
  printf(1, "Server>> listen() returned %d\n", status);

  while (1) {
    sleep(100 + uptime() % 100);

    recv(serverPort, buf, sizeof(buf));
    printf(1, "Server>> Received: \"%s\"\n", buf);

    if (0 == strcmp(buf, "exit")) {
      printf(1, "Server exiting...\n");
      disconnect(serverPort);
      break;
    }

    sleep(100 + uptime() % 100);

    strcpy(buf+strlen(buf), " OK");
    send(serverPort, buf, strlen(buf) + 1);
  }
}


int main(int argc, char *argv[])
{
  if (0 == fork()) {
    clientProc();
    exit();
  } else {
    serverProc();
    // This is the parent process. So, it needs to wait before client terminates
    wait();
    exit();
  }
}
