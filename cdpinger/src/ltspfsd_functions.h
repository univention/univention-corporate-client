/*
 * function prototypes
 */

void handle_connection(int sockfd);
void sig_chld(int signo);
void sig_term(int signo);
void eacces (int sockfd);
void handle_mount(int sockfd, XDR *in);
void handle_auth(int sockfd, XDR *in);
void ltspfs_dispatch (int sockfd, XDR *in);
void ltspfs_getattr  (int sockfd, XDR *in);
void ltspfs_readlink (int sockfd, XDR *in);
void ltspfs_readdir  (int sockfd, XDR *in);
void ltspfs_mknod    (int sockfd, XDR *in);
void ltspfs_mkdir    (int sockfd, XDR *in);
void ltspfs_symlink  (int sockfd, XDR *in);
void ltspfs_unlink   (int sockfd, XDR *in);
void ltspfs_rmdir    (int sockfd, XDR *in);
void ltspfs_rename   (int sockfd, XDR *in);
void ltspfs_link     (int sockfd, XDR *in);
void ltspfs_chmod    (int sockfd, XDR *in);
void ltspfs_chown    (int sockfd, XDR *in);
void ltspfs_truncate (int sockfd, XDR *in);
void ltspfs_utime    (int sockfd, XDR *in);
void ltspfs_open     (int sockfd, XDR *in);
void ltspfs_read     (int sockfd, XDR *in);
void ltspfs_write    (int sockfd, XDR *in);
void ltspfs_statfs   (int sockfd, XDR *in);
void ltspfs_ping     (int sockfd);
void ltspfs_quit     (int sockfd);

/*
 * Global variables
 */

extern int debug;
extern int readonly;
extern int noauth;
extern char *mountpoint;
extern int authenticated;
