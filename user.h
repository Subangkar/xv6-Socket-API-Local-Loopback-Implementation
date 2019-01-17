struct stat;
struct rtcdate;

// system calls
int fork(void);
int exit(void) __attribute__((noreturn));
int wait(void);
int pipe(int*);
int write(int, const void*, int);
int read(int, void*, int);
int close(int);
int kill(int);
int exec(char*, char**);
int open(const char*, int);
int mknod(const char*, short, short);
int unlink(const char*);
int fstat(int fd, struct stat*);
int link(const char*, const char*);
int mkdir(const char*);
int chdir(const char*);
int dup(int);
int getpid(void);
char* sbrk(int);
int sleep(int);
int uptime(void);
/// opens socket @ local-port and enables a local port to be connected from any remote socket
/// local-port
int listen(int);
/// connects to the remote port @ host is listening
/// 1:remote-port, 2:host
int connect(int, const char* host);
/// 1:local-port, 2:buffer, 3:size-of-buffer
int send(int, const char*, int);
/// 1:local-port, 2:buffer, 3:size-of-buffer
int recv(int, char*, int);
/// disconnects from the remote port and closes the socket @ local-port
/// local-port
int disconnect(int);

// ulib.c
int stat(const char*, struct stat*);
char* strcpy(char*, const char*);
void *memmove(void*, const void*, int);
char* strchr(const char*, char c);
int strcmp(const char*, const char*);
void printf(int, const char*, ...);
char* gets(char*, int max);
uint strlen(const char*);
void* memset(void*, int, uint);
void* malloc(uint);
void free(void*);
int atoi(const char*);


// console macro
#define stdin 0
#define stdout 1