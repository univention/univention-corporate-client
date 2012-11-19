/*
 * Defines
 */

/*
 * The maximum sized command we have is the symlink command.
 * We'll need: 1 BYTES_PER_XDR_UNIT for the packet length.
 *             1 BYTES_PER_XDR_UNIT for the packet type
 *             2 * (BYTES_PER_XDR_UNIT + PATH_MAX) for the paths.
 * So:
 *
 * ((4 * BYTES_PER_XDR_UNIT) + (2 * PATH_MAX))
 */

#define SERVER_PORT        9220
#define LTSP_MAXBUF        ((4 * BYTES_PER_XDR_UNIT) + (2 * PATH_MAX))
#define LTSPFS_TIMEOUT     120
#define AUTOMOUNT_TIMEOUT  5
#define LTSP_STATUS_OK     0
#define LTSP_STATUS_FAIL   1
#define LTSP_STATUS_CONT   2
#define PING_INTERVAL      30

/*
 * Packet types
 */

#define LTSPFS_GETATTR     0
#define LTSPFS_READLINK    1
#define LTSPFS_READDIR     2
#define LTSPFS_MKNOD       3
#define LTSPFS_MKDIR       4
#define LTSPFS_SYMLINK     5
#define LTSPFS_UNLINK      6
#define LTSPFS_RMDIR       7
#define LTSPFS_RENAME      8
#define LTSPFS_LINK        9
#define LTSPFS_CHMOD       10
#define LTSPFS_CHOWN       11
#define LTSPFS_TRUNCATE    12
#define LTSPFS_UTIME       13
#define LTSPFS_OPEN        14
#define LTSPFS_READ        15
#define LTSPFS_WRITE       16
#define LTSPFS_STATFS      17
#define LTSPFS_RELEASE     18		/* Not currently used */
#define LTSPFS_RSYNC       19		/* Not currently used */
#define LTSPFS_SETXATTR    20		/* Not currently used */
#define LTSPFS_GETXATTR    21		/* Not currently used */
#define LTSPFS_LISTXATTR   22		/* Not currently used */
#define LTSPFS_REMOVEXATTR 23		/* Not currently used */
#define LTSPFS_XAUTH       24
#define LTSPFS_MOUNT       25
#define LTSPFS_PING        26
#define LTSPFS_QUIT        27

