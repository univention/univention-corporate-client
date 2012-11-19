/*
 * lbmount.c: - setuid wrapper around a bindmount to allow ltsp localdevices
 *              to appear under /media
 *
 * Copyright Scott Balneaves, sbalneav@ltsp.org, 2006, 2007
 *           Martin Pitt, 2006
 *           Vagrant Cascadian, vagrant@freegeek.org, 2008
 *           Warren Togami, wtogami@redhat.com, 2008

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
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <pwd.h>
#include <mntent.h>
#include <limits.h>
#include <errno.h>
#include <getopt.h>

static uid_t uidReal;           /* Users real userid */

/*
 * Static definitions of some parts of the mount.  The program only gets passed
 * the name of the mounted device (i.e. the variable "mountpoint", and won't
 * accept any pathing on this variable.  Then, the path is built dynamically.
 * So, a bindmount from /tmp/.username-ltspfs/devicename to 
 * /media/username/devicename is made.  Below are the "static" pieces of the
 * mount strings, used later to dynamically build the full paths based on 
 * the username and mountpoint supplied. 
 */

static char *mediadir = "/media";
static char *ltspfsdir1 = "/tmp/.";
static char *ltspfsdir2 = "-ltspfs/";
static char *mountprog = "/bin/mount";  /* system mount program */
static char *umountprog = "/bin/umount";        /* system umount program */

void usage(char *progname)
{
    fprintf(stderr, "usage: %s [--umount] mediadir\n", progname);
    exit(0);
}

/*
 * is_mounted: checks a path to see if it exists as a mountpoint in /proc/mounts
 */

int is_mounted(char *path)
{
    FILE *f;
    struct mntent *entry;

    if (!(f = fopen("/proc/mounts", "r"))) {
        perror("Error: could not open /proc/mounts");
        exit(1);
    }

    while ((entry = getmntent(f)) != NULL) {
        if (!strcmp(path, entry->mnt_dir)) {
            /* OK, here's where things get a little odd.  We want to make sure
             * that the mount is owned by the user.  Normally, we'd do a stat
             * call here, but because we're a setuid program, and fuse by
             * default doesn't even allow ROOT to access users mounts, a stat
             * call will fail.  So, what we'll do is look at the "user_id=xxxx"
             * option in the mount tab, and see if it's the same as our own.
             * Clever.... veeeeery clever */
            char *pos;
            pos = strstr(entry->mnt_opts, "user_id");
            if (pos) {
                uid_t mounted_uid;
                pos += 8;                        /* skip over user_id= */
                sscanf(pos, "%d", &mounted_uid);
                if (mounted_uid == uidReal) {
                    endmntent(f);
                    return 1;
                }
            } else {
                fprintf(stderr,
                        "Error: mountpoint doesn't have user_id= in options\n");
            }
        }
    }

    fprintf(stderr, "Error: %s is not mounted\n", path);
    endmntent(f);
    return 0;
}

void mkdir_safe(char *dir)
{
    if (mkdir(dir, 0700)) {
        if (errno == EEXIST) {
            /* OK, we've been told that this already exists.  Make sure
             * nothing's mounted on top of it, so we aren't doing something
             * nasty */
            if (is_mounted(dir)) {
                fprintf(stderr,
                        "Error: can't bindmount under %s: already mounted\n",
                        dir);
                exit(1);
            }
        } else {
            perror("Unable to mkdir() in /media");
            exit(1);
        }
    }
    if (chown (dir, uidReal, 0)) {
	perror ("Unable to change owner of directory");
	rmdir (dir);
	exit (1);
    }
}

/*
 * domount: actually bindmounts path1 onto path2.
 */

int root_mounter(const char *path1, const char *path2)
{
    int status;
    pid_t child;
    char *program;
    char *null_env[] = { NULL };

    child = fork();

    if (child == 0) {
        if (setreuid(0, -1)) {
            /* Couldn't become root */
            perror("Couldn't obtain root privs");
            exit(1);
        }
        /* Statically build command line to prevent monkey business */
        if (path2)
            execle(mountprog, mountprog, "--move", path1, path2, NULL,
                   null_env);
        else
            execle(umountprog, umountprog, "-l", path1, NULL, null_env);
        perror("Error: execl() returned");
        exit(1);                                 /* exec should never return */
    } else if (child > 0) {
        if (waitpid(child, &status, 0) < 0) {
            perror("Error: wait() call failed");
            exit(1);
        }
    } else if (child < 0) {
        perror("Error: fork() failed");
        exit(1);
    }

    if (!WIFEXITED(status)) {
        fprintf(stderr, "Error: execle() returned no status");
        exit(1);
    }

    return WEXITSTATUS(status);
}


/*
 * mainline
 */

int main(int argc, char **argv)
{
    int umount = 0;
    struct passwd *pwent;
    char *mountpoint = NULL;    /* command line supplied media name */
    char mediamount[PATH_MAX];  /* fully pathed mountpoint in /media */
    char ltspfsmount[PATH_MAX]; /* fully pathed ltspfs mount in /tmp */

    int option;
    static struct option long_opts[] = {
        {"help", 0, NULL, 'h'},
        {"umount", 0, NULL, 'u'},
        {NULL, 0, NULL, 0}
    };

    /* Save our real userid */
    uidReal = getuid();

    /* Is the setuid bit set? */
    if (geteuid()) {
        perror("Error: progarm not installed setuid root");
        exit(1);
    }

    pwent = getpwuid(uidReal);
    if (!pwent) {
        perror("uid doesn't appear to have a valid passwd entry");
        exit(1);
    }

    /* Command line handling */
    do {
        option = getopt_long(argc, argv, "hu", long_opts, NULL);
        switch (option) {
        case -1:
            break;
        case 'h':
            usage(argv[0]);
            return 0;
        case 'u':
            umount = 1;
            break;
        default:
            fprintf(stderr, "%c: unknown option\n", option);
            exit(1);
        }
    }
    while (option != -1);

    /* get the optional mountpoint */
    if (optind < argc) {
        mountpoint = strdup(argv[optind]);
        if (!mountpoint) {
            fprintf(stderr, "Error: couldn't get mountpoint");
            exit(1);
        }
    } else {
        fprintf(stderr, "Error: no mountpoint supplied\n");
        exit(1);
    }

    /* security: mountpoint cannot have any /'s in it, or be too long */
    if (strlen(mountpoint) > 80 || index(mountpoint, '/')) {
        fprintf(stderr, "mountpoint name invalid.\n");
        exit(1);
    }

    /* security: make sure pwd->pw_name doesn't have a '/' in it */
    if (index(pwent->pw_name, '/')) {
        fprintf(stderr, "username invalid.\n");
        exit(1);
    }

    /*
     * Build our ltspfs mountpoint string, and check and see if it exists.
     */

    snprintf(ltspfsmount, sizeof(ltspfsmount), "%s%s%s%s",
             ltspfsdir1, pwent->pw_name, ltspfsdir2, mountpoint);

    if (!umount) {
        if (!is_mounted(ltspfsmount))
            exit(1);

        /* OK, name's a normal size, and looks valid. Begin creating the media
         * mount point. First, we need to create /media/uid */

        snprintf(mediamount, sizeof(mediamount), "%s/%s", mediadir,
                 pwent->pw_name);

        mkdir_safe(mediamount);

        /* Now, create the media dir underneath the uid dir */

        snprintf(mediamount, sizeof(mediamount), "%s/%s/%s", mediadir,
                 pwent->pw_name, mountpoint);

        mkdir_safe(mediamount);

        return root_mounter(ltspfsmount, mediamount);
    } else {
        /* umount */

        snprintf(mediamount, sizeof(mediamount), "%s/%s/%s", mediadir,
                 pwent->pw_name, mountpoint);

        if (is_mounted(mediamount))
            root_mounter(mediamount, NULL);
        else {
            fprintf(stderr, "Error: %s unmountable", mediamount);
            exit(1);
        }

        if (rmdir(mediamount)) {
            perror("Unable to rmdir() in /media");
            exit(1);
        }

        snprintf(mediamount, sizeof(mediamount), "%s/%s", mediadir,
                 pwent->pw_name);

        if (rmdir(mediamount)) {
            /* If we get an ENOTEMPTY, we can pass it by, as there's other
             * mounts still there. */
            if (errno != ENOTEMPTY) {
                perror("Unable to rmdir() users dir in /media");
                exit(1);
            }
        }

        exit(0);
    }
}
