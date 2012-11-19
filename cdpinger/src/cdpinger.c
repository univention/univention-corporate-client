/*
 * Copyright Scott Balneaves, sbalneav@ltsp.org, 2007, 2008
 *           Vagrant Cascadian, vagrant@freegeek.org, 2008
 *           Gideon Romm, gadi@ltsp.org, 2009

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
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/cdrom.h>
#include <glib.h>
#include <syslog.h>

#define POLL_INTERVAL 3				/* Check cdrom every 3 secs */

/*
 * enum for our mount states
 */

typedef enum {
    CDP_CDROM_MOUNT,
    CDP_CDROM_UMOUNT
} CdpMountType;

char *device;

/*
 * check_cd_status:  Switch mounted and unmounted states in the cdrom, and
 * call out our shell scripts to handle the actual work of plumbing the
 * ltspfs connection.
 */

gboolean
check_cd_status(void)
{
    char dev[BUFSIZ], cmd[BUFSIZ];
    int cdrom, status;
    static CdpMountType last = CDP_CDROM_UMOUNT;

    snprintf(dev, sizeof dev, "/dev/%s", device);

    /*
     * Try to open the cdrom device.  It might not exist, for a usb cdrom,
     * so simply return false so we exit.  Device has been unplugged.
     * Make sure it's unmounted, and exit.
     */

    if ((cdrom = open(dev, O_RDONLY|O_NONBLOCK)) < 0) {
	syslog (LOG_ERR, "Device %s disappeared.  Exiting.", device);
        if (last == CDP_CDROM_MOUNT) {
            snprintf(cmd, sizeof cmd,
                    "/lib/udev/ltspfs_entry remove_disc %s", device);
            g_spawn_command_line_sync(cmd, NULL, NULL, NULL, NULL);
        }
        return FALSE;
    }

    /*
     * Talk to the cdrom, get the status, and close it.
     */

    ioctl(cdrom, CDROM_LOCKDOOR, 0);
    status = ioctl (cdrom, CDROM_DRIVE_STATUS, 0);
    close(cdrom);

    /*
     * Something badly wrong with the cdrom.  Don't bother talking to it
     * anymore.
     */

    if (status < 0) {
	syslog (LOG_ERR, "ioctl(CDROM_DRIVE_STATUS) failed.  Exiting.");
        snprintf(cmd, sizeof cmd,
                "/lib/udev/ltspfs_entry remove_disc %s", device);
        return g_spawn_command_line_sync(cmd, NULL, NULL, NULL, NULL);
        return FALSE;
    }

    /*
     * Look at cdrom, see if we've got a disk or not, and mount or umount
     * appropriately
     */

    switch(status) {
        case CDS_NO_DISC:
        case CDS_TRAY_OPEN:
            if (last == CDP_CDROM_MOUNT) {
                syslog (LOG_INFO, "Tray open.  Unmounting");
                snprintf(cmd, sizeof cmd,
                        "/lib/udev/ltspfs_entry remove_disc %s", device);
                last = CDP_CDROM_UMOUNT;
                return g_spawn_command_line_sync(cmd, NULL, NULL, NULL, NULL);
            }
            break;
        case CDS_DISC_OK:
            if (last == CDP_CDROM_UMOUNT) {
                syslog (LOG_INFO, "Disk detected.  Mounting");
                snprintf(cmd, sizeof cmd,
                        "/lib/udev/ltspfs_entry add_disc %s iso9660,udf", device);
                last = CDP_CDROM_MOUNT;
                return g_spawn_command_line_sync(cmd, NULL, NULL, NULL, NULL);
            }
            break;
    }

    return TRUE;
}

/*
 * end_cd_watch: Callback routine error routine for the timeout.  If
 * check_cd_status has returned false, somthing's happened, like, say
 * the USB cdrom being unplugged, so just exit the main loop.
 */

void
end_cd_watch(gpointer loop)
{
    g_main_loop_quit((GMainLoop *)loop);
}

int
main(int argc, char *argv[])
{
    GMainLoop *loop;

    if (argc != 2)
        exit(1);

    openlog (argv[0], LOG_PID | LOG_NOWAIT, LOG_DAEMON);
    g_type_init();

    device = argv[1];

    if (daemon(0,0))
        exit(1);

    syslog (LOG_INFO, "starting, device = %s", argv[1]);

    loop = g_main_loop_new (NULL, FALSE);

    /* Add a timeout */
    g_timeout_add_full(G_PRIORITY_DEFAULT, POLL_INTERVAL * 1000,
            (GSourceFunc)check_cd_status, NULL,
	    (GDestroyNotify)end_cd_watch);

    g_main_loop_run(loop);

    closelog ();
    return 0;
}
