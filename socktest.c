#include "types.h"
#include "user.h"
#include "signalcodes.h"

void clientProc(int serverPort) {
	int clientPort;
	char buf[128];
	char host[16] = "localhost";

	// sleep for 100 clock ticks to ensure that the server process starts first.
	sleep(100);

	printf(1, "Client>> Attempting to connect to port %d, host %s ...\n", serverPort, host);
	clientPort = connect(serverPort, host);
	sleep(20);
	printf(1, "%d Client>> connect() returned %d\n", clientPort);
	while (1) {
		printf(1, "%d Client>> Enter text to send to server: ", clientPort);
		gets(buf, sizeof(buf));
		buf[strlen(buf) - 1] = '\0'; // Eliminating the '\n'
		printf(1, "%d Client>> to send to server: %s\n", clientPort, buf);
		int ret = send(clientPort, buf, strlen(buf) + 1);

		if (ret < 0 || 0 == strcmp(buf, "exit")) {
			printf(1, "%d Client exiting...(%d)\n", clientPort, ret);
			disconnect(clientPort);
			break;
		}

		sleep(100 + uptime() % 100);

		recv(clientPort, buf, sizeof(buf));
		printf(1, "%d Client>> Received: \"%s\"\n", clientPort, buf);
	}
}

void serverProc(int serverPort) {
	int status;
	char buf[128];

	printf(1, "Server>> Starting to listen at port %d ...\n", serverPort);
	status = listen(serverPort);
	printf(1, "%d Server>> listen() returned %d\n", serverPort, status);

	while (1) {
		sleep(100 + uptime() % 100);

		int ret = recv(serverPort, buf, sizeof(buf));
		printf(1, "%d Server>> Received: \"%s\"\n", serverPort, buf);

		if (ret < 0 || 0 == strcmp(buf, "exit")) {
			printf(1, "%d Server exiting...(%d)\n", serverPort, ret);
			disconnect(serverPort);
			break;
		}

		sleep(100 + uptime() % 100);

		strcpy(buf + strlen(buf), " OK");
		send(serverPort, buf, strlen(buf) + 1);
	}
}

void sockLimitTest();

int main(int argc, char *argv[]) {
//	sockLimitTest();
//	exit();
	int serverPort = 10;

	if (0 == fork()) {
		clientProc(serverPort);
		exit();
	} else {
		serverProc(serverPort);
		// This is the parent process. So, it needs to wait before client terminates
		wait();
		exit();
	}
}


#define N 2

void client(int serverPort) {
	int clientPort;
	char buf[128] = "1\n";
	char host[16] = "localhost";

	// sleep for 100 clock ticks to ensure that the server process starts first.
	sleep(100);

	printf(1, "Client>> Attempting to connect to port %d, host %s ...\n", serverPort, host);
	clientPort = connect(serverPort, host);
	sleep(20);
	printf(1, "%d Client>> connect() returned %d\n", clientPort);
	for (int i = 1; i <= N; i++) {
		buf[0] = (char) (i + '0');
		buf[1] = '\0'; // Eliminating the '\n'
		printf(1, "%d Client>> to send to server: %s\n", clientPort, buf);
		int ret = send(clientPort, buf, strlen(buf) + 1);
		if (ret < 0 || 0 == strcmp(buf, "exit")) {
			printf(1, "%d Client exiting...(%d)\n", clientPort, ret);
			disconnect(clientPort);
			break;
		}

		sleep(100 + uptime() % 100);

		recv(clientPort, buf, sizeof(buf));
		printf(1, "%d Client>> Received: \"%s\"\n", clientPort, buf);
	}
	exit();
}

void server(int serverPort) {
	int status;
	char buf[128];

	printf(1, "Server>> Starting to listen at port %d ...\n", serverPort);
	status = listen(serverPort);
	printf(1, "%d Server>> listen() returned %d\n", serverPort, status);

	for (int i = 1; i <= N; i++) {
		sleep(100 + uptime() % 100);
		int ret = recv(serverPort, buf, sizeof(buf));
		printf(1, "%d Server>> Received: \"%s\"\n", serverPort, buf);

		if (ret < 0 || 0 == strcmp(buf, "exit")) {
			printf(1, "%d Server exiting...(%d)\n", serverPort, ret);
			disconnect(serverPort);
			break;
		}

		sleep(100 + uptime() % 100);
		strcpy(buf + strlen(buf), " OK");
		send(serverPort, buf, strlen(buf) + 1);
	}
}


#define WAIT_TIME 200

void sockLimitTest() {
	if (!fork()) {
		server(24);
		exit();
	}
	sleep(WAIT_TIME);
	if (!fork()) {
		client(24);
		exit();
	}
	sleep(WAIT_TIME);
	if (!fork()) {
		server(126);
		exit();
	}
	sleep(WAIT_TIME);
	if (!fork()) {
		client(126);
		exit();
	}
	sleep(2 * WAIT_TIME);
	if (!fork()) {
		serverProc(120);
		exit();
	}
	sleep(WAIT_TIME);
	if (!fork()) {
		clientProc(120);
		exit();
	}
	sleep(WAIT_TIME);
	while (wait() > 0);
}