/*
 * Copyright Scott Balneaves, sbalneav@ltsp.org, 2006, 2007

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
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <utime.h>
#include <sys/statfs.h>
#include <unistd.h>
#include <rpc/xdr.h>
#include "ltspfs.h"
#include "ltspfsd_functions.h"
#include "common.h"

extern int mounted;

char *ltspfs_opcode_str[] = {
    "LTSPFS_GETATTR",
    "LTSPFS_READLINK",
    "LTSPFS_READDIR",
    "LTSPFS_MKNOD",
    "LTSPFS_MKDIR",
    "LTSPFS_SYMLINK",
    "LTSPFS_UNLINK",
    "LTSPFS_RMDIR",
    "LTSPFS_RENAME",
    "LTSPFS_LINK",
    "LTSPFS_CHMOD",
    "LTSPFS_CHOWN",
    "LTSPFS_TRUNCATE",
    "LTSPFS_UTIME",
    "LTSPFS_OPEN",
    "LTSPFS_READ",
    "LTSPFS_WRITE",
    "LTSPFS_STATFS",
    "LTSPFS_RELEASE",
    "LTSPFS_RSYNC",
    "LTSPFS_SETXATTR",
    "LTSPFS_GETXATTR",
    "LTSPFS_LISTXATTR",
    "LTSPFS_REMOVEXATTR",
    "LTSPFS_XAUTH",
    "LTSPFS_MOUNT",
    "LTSPFS_PING",
    "LTSPFS_QUIT"
};

/*
 * eacces:
 *
 * This function stub is assigned to several callouts when read-only mode
 * is enabled.  It basically just returns EACCES to the client FUSE program.
 */

void eacces(int sockfd)
{
    if (debug)
        info("eacces called\n");
    errno = EACCES;
    status_return(sockfd, FAIL);
}

/*
 * Helper routine to do get a path from the XDR stream, and do all the path
 * adjustment.
 */

int get_fn(int sockfd, XDR * in, char *path)
{
    char *pathptr = path;
    int mpl;

    mpl = strlen(mountpoint);

    strncpy(path, mountpoint, mpl);

    pathptr += mpl;

    if (!xdr_string(in, &pathptr, (PATH_MAX - mpl))) {
        eacces(sockfd);
        return FAIL;
    } else
        return OK;
}

/*
 * ltspfs_dispatch:
 *
 * The callout dispatcher.  Once a command's been read from the socket,
 * this function figures out which command it is, and dispatches the
 * correct handler function.
 */

void ltspfs_dispatch(int sockfd, XDR * in)
{
    int packet_type;

    if (!xdr_int(in, &packet_type)) {
        if (debug)
            info("Packet type decode failed!\n");
        close(sockfd);
        exit(1);
    }

    if (debug)
        info("Packet type: %s\n", ltspfs_opcode_str[packet_type]);

    if (noauth) {
        authenticated++;
    }

    if (!authenticated) {                        /* Haven't authenticated yet */
        switch (packet_type) {
        case LTSPFS_XAUTH:
            handle_auth(sockfd, in);
            break;
        default:
            status_return(sockfd, FAIL);
        }
    } else if (!mountpoint) {
        switch (packet_type) {
        case LTSPFS_MOUNT:
            handle_mount(sockfd, in);            /* Haven't mounted yet */
            break;
        default:
            status_return(sockfd, FAIL);
        }
    } else if (packet_type == LTSPFS_PING) {
        ltspfs_ping(sockfd);
    } else {
        if (!mounted)
            am_mount(mountpoint);                /* this will return */

        switch (packet_type) {
        case LTSPFS_GETATTR:
            ltspfs_getattr(sockfd, in);
            break;
        case LTSPFS_READLINK:
            ltspfs_readlink(sockfd, in);
            break;
        case LTSPFS_READDIR:
            ltspfs_readdir(sockfd, in);
            break;
        case LTSPFS_MKNOD:
            ltspfs_mknod(sockfd, in);
            break;
        case LTSPFS_MKDIR:
            ltspfs_mkdir(sockfd, in);
            break;
        case LTSPFS_SYMLINK:
            ltspfs_symlink(sockfd, in);
            break;
        case LTSPFS_UNLINK:
            ltspfs_unlink(sockfd, in);
            break;
        case LTSPFS_RMDIR:
            ltspfs_rmdir(sockfd, in);
            break;
        case LTSPFS_RENAME:
            ltspfs_rename(sockfd, in);
            break;
        case LTSPFS_LINK:
            ltspfs_link(sockfd, in);
            break;
        case LTSPFS_CHMOD:
            ltspfs_chmod(sockfd, in);
            break;
        case LTSPFS_CHOWN:
            ltspfs_chown(sockfd, in);
            break;
        case LTSPFS_TRUNCATE:
            ltspfs_truncate(sockfd, in);
            break;
        case LTSPFS_UTIME:
            ltspfs_utime(sockfd, in);
            break;
        case LTSPFS_OPEN:
            ltspfs_open(sockfd, in);
            break;
        case LTSPFS_READ:
            ltspfs_read(sockfd, in);
            break;
        case LTSPFS_WRITE:
            ltspfs_write(sockfd, in);
            break;
        case LTSPFS_STATFS:
            ltspfs_statfs(sockfd, in);
            break;
        case LTSPFS_RELEASE:
        case LTSPFS_RSYNC:
        case LTSPFS_SETXATTR:
        case LTSPFS_GETXATTR:
        case LTSPFS_LISTXATTR:
        case LTSPFS_REMOVEXATTR:
            break;
        case LTSPFS_QUIT:
            ltspfs_quit(sockfd);
            break;
        default:
            status_return(sockfd, FAIL);
            if (debug)
                info("Invalid command: %d\n", packet_type);
        }
    }
}

/*
 * ltspfs_getattr:
 *
 * sends the output of the lstat() system call back to the client FUSE program
 */

void ltspfs_getattr(int sockfd, XDR * in)
{
    XDR out;
    char path[PATH_MAX];
    char output[LTSP_MAXBUF];
    int i;
    struct stat stbuf;
    unsigned int nlink;

    if (get_fn(sockfd, in, path)) {
        if (debug)
            info("get_fn failed\n");
        eacces(sockfd);
        return;
    }

    if (lstat(path, &stbuf) == -1) {
        status_return(sockfd, FAIL);
        return;
    }

    xdrmem_create(&out, output, LTSP_MAXBUF, XDR_ENCODE);
    i = 0;
    xdr_int(&out, &i);                           /* First, the dummy length */
    xdr_int(&out, &i);                           /* Then the 0 status return */
    xdr_u_longlong_t(&out, &(stbuf.st_dev));     /* device */
    xdr_u_longlong_t(&out, &(stbuf.st_ino));     /* inode */
    xdr_u_int(&out, &(stbuf.st_mode));           /* protection */
    xdr_u_int(&out, &nlink);
    stbuf.st_nlink = (nlink_t) nlink;
    xdr_u_int(&out, &(stbuf.st_uid));            /* user ID of owner */
    xdr_u_int(&out, &(stbuf.st_gid));            /* group ID of owner */
    xdr_u_longlong_t(&out, &(stbuf.st_rdev));    /* device type */
    xdr_longlong_t(&out, &(stbuf.st_size));      /* total size, in bytes */
    xdr_long(&out, &(stbuf.st_blksize));         /* blocksize for fs I/O */
    xdr_longlong_t(&out, &(stbuf.st_blocks));    /* num of blocks allocated */
    xdr_long(&out, &(stbuf.st_atime));           /* time of last access */
    xdr_long(&out, &(stbuf.st_mtime));           /* time of last modification */
    xdr_long(&out, &(stbuf.st_ctime));           /* time of last status chng */
    i = xdr_getpos(&out);                        /* Get our position */
    xdr_setpos(&out, 0);                         /* Rewind to the beginning */
    xdr_int(&out, &i);                           /* Rewrite w/ proper length */
    xdr_destroy(&out);

    if (debug)
        info("returning OK");

    writen(sockfd, output, i);
}

/*
 * ltspfs_readlink:
 *
 * sends the link target returned by readlink() to the client FUSE program
 */

void ltspfs_readlink(int sockfd, XDR * in)
{
    XDR out;
    char path[PATH_MAX];
    char buf[PATH_MAX];                          /* linkname */
    char output[LTSP_MAXBUF];
    char *bufptr = buf;
    int i;

    /* readlink doesn't terminate with a null */
    memset(buf, 0, PATH_MAX);

    if (get_fn(sockfd, in, path)) {              /* Get the link source */
        eacces(sockfd);
        return;
    }

    if (readlink(path, buf, PATH_MAX) == -1) {
        status_return(sockfd, FAIL);
        return;
    }

    if (!strncmp(buf, mountpoint, strlen(mountpoint)))  /* adjust link target */
        bufptr += strlen(mountpoint);

    xdrmem_create(&out, output, LTSP_MAXBUF, XDR_ENCODE);
    i = 0;
    xdr_int(&out, &i);                           /* First, the dummy length */
    xdr_int(&out, &i);                           /* Then the 0 status return */
    xdr_string(&out, &bufptr, PATH_MAX);         /* Link target */
    i = xdr_getpos(&out);                        /* Get our position */
    xdr_setpos(&out, 0);                         /* Rewind to the beginning */
    xdr_int(&out, &i);                           /* Rewrite with proper length */
    xdr_destroy(&out);

    if (debug)
        info("returning ok", output);

    writen(sockfd, output, i);
}

/*
 * ltspfs_readdir:
 *
 * Returns the directory listing to the client FUSE program.  Output looks
 * like:
 *
 * 100|<inode1>|<type1>|<filename1>
 * 100|<inode2>|<type2>|<filename2>
 * ...
 * 100|<inodeN>|<typeN>|<filenameN>
 * 001
 */

void ltspfs_readdir(int sockfd, XDR * in)
{
    XDR out;
    char path[PATH_MAX];
    char output[LTSP_MAXBUF];
    DIR *dp;
    char *nameptr;
    struct dirent *de;
    int i;

    if (get_fn(sockfd, in, path)) {              /* Get the dir name */
        eacces(sockfd);
        return;
    }

    dp = opendir(path);

    if (dp == NULL) {
        status_return(sockfd, FAIL);             /* opendir failed */
        return;
    }

    while ((de = readdir(dp)) != NULL) {
        xdrmem_create(&out, output, LTSP_MAXBUF, XDR_ENCODE);
        i = 0;
        xdr_int(&out, &i);                       /* First, the dummy length */
        i = LTSP_STATUS_CONT;
        xdr_int(&out, &i);                       /* Then the 2 status return */
        xdr_u_longlong_t(&out, &(de->d_ino));    /* Inode */
        xdr_u_char(&out, &(de->d_type));         /* type */
        nameptr = de->d_name;
        xdr_string(&out, &nameptr, PATH_MAX);    /* filename */
        i = xdr_getpos(&out);                    /* Get our position */
        xdr_setpos(&out, 0);                     /* Rewind to the beginning */
        xdr_int(&out, &i);                       /* Rewrite w/ proper length */
        xdr_destroy(&out);

        if (debug)
            info("returning %s", de->d_name);

        writen(sockfd, output, i);
    }

    closedir(dp);

    status_return(sockfd, OK);
}

/*
 * ltspfs_mknod:
 *
 * Makes filesystem nodes (regular files, pipes, device nodes, etc).
 */

void ltspfs_mknod(int sockfd, XDR * in)
{
    char path[PATH_MAX];
    mode_t mode;
    dev_t rdev;

    if (!xdr_u_int(in, &mode)) {                 /* Get the mode */
        eacces(sockfd);
        return;
    }

    if (!xdr_u_longlong_t(in, &rdev)) {          /* Get the device type */
        eacces(sockfd);
        return;
    }

    if (get_fn(sockfd, in, path)) {              /* Get the dir name */
        eacces(sockfd);
        return;
    }

    status_return(sockfd, mknod(path, mode, rdev));
}

/*
 * ltspfs_mkdir:
 *
 * Makes directories
 */

void ltspfs_mkdir(int sockfd, XDR * in)
{
    char path[PATH_MAX];
    mode_t mode;

    if (!xdr_u_int(in, &mode)) {                 /* Get the mode */
        eacces(sockfd);
        return;
    }

    if (get_fn(sockfd, in, path)) {              /* Get the dir name */
        eacces(sockfd);
        return;
    }

    status_return(sockfd, mkdir(path, mode));
}

/*
 * ltspfs_symlink:
 *
 * Makes symlinks
 */

void ltspfs_symlink(int sockfd, XDR * in)
{
    char from[PATH_MAX];
    char to[PATH_MAX];

    if (get_fn(sockfd, in, from)) {              /* Get the source link */
        eacces(sockfd);
        return;
    }

    if (get_fn(sockfd, in, to)) {                /* Get the destination link */
        eacces(sockfd);
        return;
    }


    status_return(sockfd, symlink(from, to));
}

/*
 * ltspfs_unlink:
 *
 * Deletes (unlinks) files.
 */

void ltspfs_unlink(int sockfd, XDR * in)
{
    char path[PATH_MAX];

    if (get_fn(sockfd, in, path)) {              /* Get the path */
        eacces(sockfd);
        return;
    }

    status_return(sockfd, unlink(path));
}

/*
 * ltspfs_rmdir:
 *
 * Removes directories.
 */

void ltspfs_rmdir(int sockfd, XDR * in)
{
    char path[PATH_MAX];

    if (get_fn(sockfd, in, path)) {              /* Get the path */
        eacces(sockfd);
        return;
    }

    status_return(sockfd, rmdir(path));
}

/*
 * ltspfs_rename:
 *
 * Renames files.
 */

void ltspfs_rename(int sockfd, XDR * in)
{
    char from[PATH_MAX];
    char to[PATH_MAX];

    if (get_fn(sockfd, in, from)) {              /* Get the source name */
        eacces(sockfd);
        return;
    }

    if (get_fn(sockfd, in, to)) {                /* Get the destination name */
        eacces(sockfd);
        return;
    }

    status_return(sockfd, rename(from, to));
}

/*
 * ltspfs_link:
 *
 * Creates hard links.
 */

void ltspfs_link(int sockfd, XDR * in)
{
    char from[PATH_MAX];
    char to[PATH_MAX];

    if (get_fn(sockfd, in, from)) {              /* Get the source link */
        eacces(sockfd);
        return;
    }
    if (get_fn(sockfd, in, to)) {                /* Get the destination link */
        eacces(sockfd);
        return;
    }

    status_return(sockfd, link(from, to));
}

/*
 * ltspfs_chmod:
 *
 * Changes permisions on files.
 */

void ltspfs_chmod(int sockfd, XDR * in)
{
    char path[PATH_MAX];
    mode_t mode;

    if (!xdr_u_int(in, &mode)) {                 /* Get the mode */
        eacces(sockfd);
        return;
    }

    if (get_fn(sockfd, in, path)) {              /* Get the path */
        eacces(sockfd);
        return;
    }

    status_return(sockfd, chmod(path, mode));
}

/*
 * ltspfs_chown:
 *
 * Changes owners/groups on files.
 */

void ltspfs_chown(int sockfd, XDR * in)
{
    char path[PATH_MAX];
    uid_t uid;
    gid_t gid;

    if (!xdr_u_int(in, &uid)) {                  /* Get the mode */
        eacces(sockfd);
        return;
    }

    if (!xdr_u_int(in, &gid)) {                  /* Get the mode */
        eacces(sockfd);
        return;
    }

    if (get_fn(sockfd, in, path)) {              /* Get the path */
        eacces(sockfd);
        return;
    }

    status_return(sockfd, chown(path, uid, gid));
}

/*
 * ltspfs_truncate:
 *
 * Truncates a file at offset bytes.
 */

void ltspfs_truncate(int sockfd, XDR * in)
{
    char path[PATH_MAX];
    off_t offset;

    if (!xdr_longlong_t(in, &offset)) {          /* Get the mode */
        eacces(sockfd);
        return;
    }

    if (get_fn(sockfd, in, path)) {              /* Get the path */
        eacces(sockfd);
        return;
    }

    status_return(sockfd, truncate(path, offset));
}

/*
 * ltspfs_utime:
 *
 * Updates the access time and modification time of a file
 */

void ltspfs_utime(int sockfd, XDR * in)
{
    char path[PATH_MAX];
    struct utimbuf timbuf;

    if (!xdr_long(in, &timbuf.actime)) {         /* Get the actime */
        eacces(sockfd);
        return;
    }

    if (!xdr_long(in, &timbuf.modtime)) {        /* Get the modtime */
        eacces(sockfd);
        return;
    }

    if (get_fn(sockfd, in, path)) {              /* Get the path */
        eacces(sockfd);
        return;
    }

    status_return(sockfd, utime(path, &timbuf));
}

/*
 * ltspfs_open:
 *
 * Tests whether or not a file may be opened dependant on the flags.
 */

void ltspfs_open(int sockfd, XDR * in)
{
    char path[PATH_MAX];
    int result;
    int flags;

    if (!xdr_int(in, &flags)) {                  /* Get the flags */
        eacces(sockfd);
        return;
    }

    if (get_fn(sockfd, in, path)) {              /* Get the path */
        eacces(sockfd);
        return;
    }

    /*
     * If we're a read-only filesystem, and we've been asked to open
     * O_WRONLY or O_RDWR, then return EACCES
     */

    if (readonly)
        if ((flags & O_WRONLY) || (flags & O_RDWR)) {
            eacces(sockfd);
            return;
        }

    /*
     * Otherwise, check the permissions with a test open, and return the
     * results if an error occurred.
     */

    result = open(path, flags);

    status_return(sockfd, result);

    if (result != -1)
        close(result);
}

/*
 * ltspfs_read:
 *
 * Reads blocks from a file.  Atomic: open-seek-read-close
 */

void ltspfs_read(int sockfd, XDR * in)
{
    XDR out;
    char path[PATH_MAX];
    char output[LTSP_MAXBUF];
    int i;
    int fd;
    int result;
    unsigned int usize;
    size_t size;
    off_t offset;
    char *buf;

    if (!xdr_u_int(in, &usize)) {                 /* Get the size */
        eacces(sockfd);
        return;
    }

    size = (size_t) usize;

    if (!xdr_longlong_t(in, &offset)) {          /* Get the offset */
        eacces(sockfd);
        return;
    }

    if (get_fn(sockfd, in, path)) {              /* Get the path */
        eacces(sockfd);
        return;
    }


    buf = malloc(size);

    /*
     * Check result of malloc
     */

    if (!buf) {
        status_return(sockfd, FAIL);
        return;
    }

    fd = open(path, O_RDONLY);
    if (fd == -1) {
        status_return(sockfd, FAIL);
        free(buf);
        return;
    }

    lseek(fd, offset, SEEK_SET);
    result = read(fd, buf, size);

    if (result < 0)
        status_return(sockfd, FAIL);
    else {
        xdrmem_create(&out, output, LTSP_MAXBUF, XDR_ENCODE);
        i = 0;
        xdr_int(&out, &i);                       /* dummy length */
        i = LTSP_STATUS_OK;                      /* OK status */
        xdr_int(&out, &i);
        xdr_int(&out, &result);                  /* Write out the result */
        i = xdr_getpos(&out);                    /* Get current position */
        xdr_setpos(&out, 0);                     /* rewind to the beginning */
        xdr_int(&out, &i);                       /* re-write proper length */
        xdr_destroy(&out);                       /* Clean up the XDR structs */

        if (debug)
            info("read returning %d bytes", result);

        writen(sockfd, output, i);               /* Write out status */

        writen(sockfd, buf, result);             /* Write out data payload */
    }

    close(fd);

    free(buf);
}

void ltspfs_write(int sockfd, XDR * in)
{
    XDR out;
    char path[PATH_MAX];
    char output[LTSP_MAXBUF];
    int i;
    int fd;
    int result;
    size_t size;
    unsigned int usize;
    off_t offset;
    char *buf;

    if (!xdr_u_int(in, &usize)) {                /* Get the size */
        eacces(sockfd);
        return;
    }

    size = (size_t) usize;

    if (!xdr_longlong_t(in, &offset)) {          /* Get the offset */
        eacces(sockfd);
        return;
    }

    if (get_fn(sockfd, in, path)) {              /* Get the path */
        eacces(sockfd);
        return;
    }


    buf = malloc(size);

    /*
     * Check result of malloc
     */

    if (!buf) {
        status_return(sockfd, FAIL);
        return;
    }

    fd = open(path, O_WRONLY);
    if (fd == -1) {
        status_return(sockfd, FAIL);
        free(buf);
        return;
    }

    readn(sockfd, buf, size);

    lseek(fd, offset, SEEK_SET);
    result = write(fd, buf, size);

    if (result < 0)
        status_return(sockfd, FAIL);
    else {
        xdrmem_create(&out, output, LTSP_MAXBUF, XDR_ENCODE);
        i = 0;
        xdr_int(&out, &i);                       /* dummy length */
        i = LTSP_STATUS_OK;                      /* OK status */
        xdr_int(&out, &i);
        xdr_int(&out, &result);                  /* Write out the result */
        i = xdr_getpos(&out);                    /* Get current position */
        xdr_setpos(&out, 0);                     /* rewind to the beginning */
        xdr_int(&out, &i);                       /* re-write proper length */
        xdr_destroy(&out);                       /* Clean up the XDR structs */

        if (debug)
            info("returning %s", output);

        writen(sockfd, output, i);
    }

    close(fd);

    free(buf);
}

void ltspfs_statfs(int sockfd, XDR * in)
{
    XDR out;
    char path[PATH_MAX];
    char output[LTSP_MAXBUF];
    struct statfs stbuf;
    int i;
    int f_type, f_bsize, f_namelen;

    if (get_fn(sockfd, in, path)) {              /* Get the path */
        eacces(sockfd);
        return;
    }


    if (statfs(path, &stbuf) == -1) {
        status_return(sockfd, FAIL);
        return;
    }

    xdrmem_create(&out, output, LTSP_MAXBUF, XDR_ENCODE);
    i = 0;
    xdr_int(&out, &i);                           /* dummy length */
    i = LTSP_STATUS_OK;                          /* OK status */
    xdr_int(&out, &i);
    xdr_int(&out, &f_type);                      /* type of fs */
    xdr_int(&out, &f_bsize);                     /* optimal transfer block sz */
    xdr_u_longlong_t(&out, &stbuf.f_blocks);     /* total data blocks in fs */
    xdr_u_longlong_t(&out, &stbuf.f_bfree);      /* free blks in fs */
    xdr_u_longlong_t(&out, &stbuf.f_bavail);     /* free blks avail to non-su */
    xdr_u_longlong_t(&out, &stbuf.f_files);      /* total file nodes in fs */
    xdr_u_longlong_t(&out, &stbuf.f_ffree);      /* free file nodes in fs */
    xdr_int(&out, &f_namelen);                   /* max length of filenames */
    stbuf.f_type = (__SWORD_TYPE) f_type;
    stbuf.f_bsize = (__SWORD_TYPE) f_bsize;
    stbuf.f_namelen = (__SWORD_TYPE) f_namelen;
    i = xdr_getpos(&out);                        /* Get current position */
    xdr_setpos(&out, 0);                         /* rewind to the beginning */
    xdr_int(&out, &i);                           /* re-write proper length */
    xdr_destroy(&out);                           /* Clean up the XDR structs */

    if (debug)
        info("returning OK");

    writen(sockfd, output, i);
}

/*
 * ltspfs_ping:
 *
 * Transmit a simple "000" status return to a ping request.
 */

void ltspfs_ping(int sockfd)
{
    status_return(sockfd, OK);

    if (debug)
        info("received ping packet\n");
}

void ltspfs_quit(int sockfd)
{
    /*
     * Close up shop and bail.
     */

    close(sockfd);
    exit(OK);
}

void handle_mount(int sockfd, XDR * in)
{
    char path[PATH_MAX];
    char *pathptr = path;

    /*
     * Get our mount point
     */

    if (!xdr_string(in, &pathptr, PATH_MAX)) {   /* Get the path */
        eacces(sockfd);
        return;
    }

    /*
     * Here's where you'd do sanity checking on the dir, checking an exports 
     * file, etc.  For now, we'll assume it's correct.
     */

    mountpoint = strdup(path);

    if (debug)
        info("mount: %s\n", mountpoint);

    status_return(sockfd, OK);
}

/*
 * handle_auth
 *
 * Read our secret code (generated with mcookie), and compare it to what the
 * client sent us.
 */

void handle_auth(int sockfd, XDR * in)
{
    char recdtoken[64];
    int auth_size;
    char authtoken[64];
    int authtokensize;
    int authfile, i;

    /*
     * Get our auth size.
     */

    if (!xdr_int(in, &auth_size)) {            /* Get the size */
        eacces(sockfd);
        return;
    }

    if (auth_size >= (int) sizeof recdtoken) {
        eacces(sockfd);
        return;
    }

    readn(sockfd, recdtoken, auth_size);         /* read packet */

    recdtoken[auth_size] = '\0';

    /*
     * Test our authentication.
     * our XAUTHORITY environment variable is pointing to the xauth record,
     * so if we've got a match, we the open should succeed.
     */

    authfile = open("/var/run/ltspfs_token", O_RDONLY);
    if (authfile < 0) {
        eacces(sockfd);
        return;
    }

    authtokensize = read(authfile, authtoken, sizeof authtoken);
    close(authfile);

    for (i = 0; i < authtokensize; i++)
        if (authtoken[i] == '\n')
            authtoken[i] = '\0';

    if (strncmp(authtoken, recdtoken, auth_size)) {
        eacces(sockfd);
        return;
    }

    status_return(sockfd, OK);                   /* Acknowledge auth */
    authenticated++;                             /* Set auth state */
}
