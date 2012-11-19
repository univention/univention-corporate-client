/*
 * defines
 */

#define OK    0
#define ERROR 1
#define FAIL -1

#define SEP   1
#define NOSEP 0
#define BACKLOG 5               /* We can be backlogged up to 5 connections */

/*
 * External variables
 */

extern int syslogopen;
extern int debug;

/*
 * function prototypes
 */

int bindsocket(int port);
int opensocket(char *hostname, int port);
int readn(register int fd, register char *ptr, register int nbytes);
int writen(register int fd, register char *ptr, register int nbytes);
int _readn(register int fd, register char *ptr, register int nbytes,
          int timeout_secs, void (*timeout_function)(), int doselect);
int _writen(register int fd, register char *ptr, register int nbytes,
          int timeout_secs, void (*timeout_function)(), int doselect);
int streq (char *s1, char *s2);
void timeout();

int status_return(int sockfd, int result);
void error_die(char *err);
void info(const char *format, ...);

void am_mount(char *mountpoint);
void am_umount(char *mountpoint);
