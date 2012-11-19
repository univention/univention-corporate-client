/*
 * common.c: support routines for ltspfs.
 *

 * Copyright Scott Balneaves, sbalneav@ltsp.org, 2005, 2006, 2007

 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, you can find it on the World Wide
 * Web at http://www.gnu.org/copyleft/gpl.html, or write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02110-1301, USA.

 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <rpc/xdr.h>
#include "ltspfs.h"
#include "common.h"

/*
 * Open up a server socket.
 */

extern int mounted;

int bindsocket(int port)
{
    struct sockaddr_in serv_addr;
    int s;

    /*
     * Open up our socket
     */

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
        error_die("Can't open socket");

    /*
     *  bind server port
     */

    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    if (bind(s, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error_die("Can't bind port");

    /*
     * Listen for connections
     */

    if (listen(s, BACKLOG) < 0)
        error_die("Listen failed");

    return s;
}

/*
 * Open up a client socket.
 */

int opensocket(char *hostname, int port)
{
    struct sockaddr_in serv_addr;
    struct hostent *server;
    int s;

    /*
     * Open up our socket
     */

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        fprintf(stderr, "ERROR opening socket\n");
        exit(1);
    }

    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host\n");
        exit(1);
    }

    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *) server->h_addr,
          (char *) &serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(port);
    if (connect(s, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        fprintf(stderr, "ERROR connecting\n");
        exit(1);
    }

    return s;
}

/*
 * _readn: read n bytes from the socket
 * The select() function is used to handle timeouts.  On a timeout, the
 * timeout_function is called.
 */

int
_readn(register int fd, register char *ptr, register int nbytes,
       int timeout_secs, void (*timeout_function) (), int doselect)
{
    int nleft, nread;
    int r;
    fd_set set;                 /* For select */
    struct timeval ltspfs_timeout;      /* Timeout */
    struct timeval *timeout_ptr = &ltspfs_timeout;

    FD_ZERO(&set);
    FD_SET(fd, &set);

    if (!doselect)
        timeout_ptr = NULL;

    nleft = nbytes;

    while (nleft > 0) {
        ltspfs_timeout.tv_sec = timeout_secs;
        ltspfs_timeout.tv_usec = 0;
        r = select(FD_SETSIZE, &set, NULL, NULL, timeout_ptr);
        if (r < 0)
            return r;
        else if (r == 0)
            timeout_function();                  /* this should never return */
        else {
            nread = read(fd, ptr, nleft);
            if (nread < 0)
                return (nread);                  /* error, return < 0 */
            else if (nread == 0)
                break;                           /* EOF */

            nleft -= nread;
            ptr += nread;
        }
    }

    return (nbytes - nleft);                     /* return >= 0 */
}

/*
 * writen: write n bytes to the socket
 */

int
_writen(register int fd, register char *ptr, register int nbytes,
        int timeout_secs, void (*timeout_function) (), int doselect)
{
    int nleft, nwritten;
    int r;
    fd_set set;                                  /* For select */
    struct timeval ltspfs_timeout;               /* Timeout */
    struct timeval *timeout_ptr = &ltspfs_timeout;

    FD_ZERO(&set);
    FD_SET(fd, &set);

    if (!doselect)
        timeout_ptr = NULL;

    nleft = nbytes;

    while (nleft > 0) {
        ltspfs_timeout.tv_sec = timeout_secs;
        ltspfs_timeout.tv_usec = 0;
        r = select(FD_SETSIZE, NULL, &set, NULL, timeout_ptr);
        if (r < 0)
            return r;
        else if (r == 0)
            timeout_function();                  /* it's expected that this will never return */
        else {
            nwritten = write(fd, ptr, nleft);
            if (nwritten <= 0)
                return (nwritten);               /* error, return < 0 */

            nleft -= nwritten;
            ptr += nwritten;
        }
    }

    return (nbytes - nleft);                     /* return >= 0 */
}

/*
 * These are the user functions.  They handle some things with less parameters.
 */

int readn(register int fd, register char *ptr, register int maxlen)
{
    return _readn(fd, ptr, maxlen, LTSPFS_TIMEOUT, &timeout, TRUE);
}

int writen(register int fd, register char *ptr, register int nbytes)
{
    return _writen(fd, ptr, nbytes, LTSPFS_TIMEOUT, &timeout, TRUE);
}

#ifdef AUTOMOUNT
/*
 * Automounter.
 */

void am_mount(char *mountpoint)
{
    struct stat buf;
    char *path[] = { "/usr/sbin/ltspfs_mount", mountpoint, NULL };
    pid_t pid;
    int status;


    if (debug)
        info("am_mount called\n");
    if (mounted)                                 /* buh? */
        return;

    /*
     * Check if the mount script exists before calling
     */

    if (!stat(path[0], &buf)) {
        pid = fork();
        if (pid == 0) {
            execv(path[0], path);
            perror("execv");
            exit(1);
        } else if (pid < 0) {
            perror("fork");
            exit(1);
        }

        wait(&status);
    }
    mounted = 1;
}

void am_umount(char *mountpoint)
{
    struct stat buf;
    char *path[] = { "/usr/sbin/ltspfs_umount", mountpoint, NULL };
    pid_t pid;
    int status;


    if (debug)
        info("am_umount called\n");
    if (!mounted)                                /* buh? */
        return;

    /*
     * Check if the umount script exists before calling
     */

    if (!stat(path[0], &buf)) {
        pid = fork();
        if (pid == 0) {
            execv(path[0], path);
            perror("execv");
            exit(1);
        } else if (pid < 0) {
            perror("fork");
            exit(1);
        }

        wait(&status);
    }
    mounted = 0;
}

#endif

/*
 * status_return is used to simplify a bunch of functions that either
 * return a simple error, or an all clear error code.
 */

int status_return(int sockfd, int result)
{
    XDR out;
    char output[BUFSIZ];
    int i = 0;

    xdrmem_create(&out, output, BUFSIZ, XDR_ENCODE);
    xdr_int(&out, &i);                           /* bogus length */

    if (result == FAIL) {
        i = LTSP_STATUS_FAIL;
        xdr_int(&out, &i);
        if (debug)
            info("status_return STATUS_FAIL\n");
        xdr_int(&out, &errno);
    } else {
        i = LTSP_STATUS_OK;
        if (debug)
            info("status_return STATUS_OK\n");
        xdr_int(&out, &i);
    }

    i = xdr_getpos(&out);                        /* get current position */
    xdr_setpos(&out, 0);                         /* rewind to the beginning */
    xdr_int(&out, &i);                           /* Write the correct length */


    writen(sockfd, output, i);
    return 0;
}

/*
 * error_die:
 * Issue an error to syslog and die.
 */

void error_die(char *err)
{
    info("%s (errno = %d : %s): Shutting down\n", err, errno,
         strerror(errno));
    exit(ERROR);
}

/*
 * info: logs messages to syslog and/or stderr.
 *
 * Thanks to Petter Reinholdtsen for the heads up on the v... functions.
 */

void info(const char *format, ...)
{
    va_list ap;

    va_start(ap, format);

    if (syslogopen)
        vsyslog(LOG_INFO, format, ap);
    if (debug)
        vfprintf(stderr, format, ap);

    va_end(ap);
}
