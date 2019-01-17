
enum sockstate { CLOSED, LISTENING, CONNECTED };



// Per-socket state
struct sock {

	int sid;                     // Socket ID
	enum sockstate state;        // Socket's current state

	void *chan;                  // If non-zero, sleeping on chan

	struct proc *owner;          // Socket owner process

	char recvbuffer[SBUFFSIZE];  // socket buffer : current buffer size to be determined by EOF
	bool hasUnreadData;          //

	int lPort;
	int rPort;
};
// {-1,1,CLOSED,NULL,NULL,"",false,-1,-1}

#define INV_PORT -1

#define DISCARD_INV_PORT(port) if (port < 0 || port >= NPORT) return E_INVALID_ARG;

#define SOCK_SID_DEF -1
#define SOCK_STATE_DEF CLOSED
#define SOCK_CHAN_DEF NULL
#define SOCK_OWNER_DEF NULL
#define SOCK_BUFFER_DEF ""
#define SOCK_HASDATA_DEF false
#define SOCK_LPORT_DEF INV_PORT
#define SOCK_RPORT_DEF INV_PORT