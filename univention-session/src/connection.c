/*
 * Univention Client Login
 *	this file is part of the Univention thin client tools
 *
 * Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006 Univention GmbH
 *
 * http://www.univention.de/
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Binary versions of this file provided by Univention to you as
 * well as other copyrighted, protected or trademarked materials like
 * Logos, graphics, fonts, specific documentations and configurations,
 * cryptographic keys etc. are subject to a license agreement between
 * you and Univention.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>
#include <signal.h>
#include <unistd.h>
#include <wait.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

extern int errno;

#include "debug.h"
#include "process.h"
#include "protocol.h"
#include "command.h"
#include "select_loop.h"

extern int should_exit;


int send_fd = -1;
int recv_fd = -1;

int rsh_pid = -1;

int connection_read_handler ( int fd )
{
	char buf[MAX_CMD_LEN];

	if ( debug_level )
		debug_printf ( "connection_read_handler\n" );
	recv_command ( buf, MAX_CMD_LEN );
	if ( debug_level )
		debug_printf ( "received command: %s\n", buf );
	execute_command ( buf );

	return 0;
}

/*
 * Move file descriptor and mark as CLOSE-ON-EXEC.
 */
static int move_fd(int oldfd, int newfd) {
	int flags;
	int tmpfd;

	if (oldfd != newfd)
		tmpfd = dup2(oldfd, newfd);
	else
		tmpfd = newfd;
	if (tmpfd != newfd)
		fatal_perror("Error dup2 failed\n");
	flags = fcntl(tmpfd, F_GETFD);
	if (flags == -1)
		fatal_perror("Error getting tmpfd status\n");
	flags |= FD_CLOEXEC;
	if (fcntl(tmpfd, F_SETFD, flags) == -1)
		fatal_perror("Error setting tmpfd status\n");

	return tmpfd;
}

/*
 * Open /dev/null on fd in mode given by flags.
 */
static void open_dev_null(int fd, int flags) {
	int tmpfd;

	close(fd); /* ignore error if already closed. */
	tmpfd = open("/dev/null", flags);
	if (tmpfd < 0)
		fatal_perror("Error opening /dev/null\n");
	if (tmpfd != fd) {
		int ret;

		ret = dup2(tmpfd, fd);
		if (ret < 0)
			fatal_perror("Error dup2 failed\n");
		ret = close(tmpfd);
		if (ret < 0)
			fatal_perror("Error closing fd\n");
	}
}

/*
 * Clone FDs for client<->server communication inherited from SESSION_RSH
 * to private FDs and mark them as CLOSE-ON-EXEC.
 * Re-open STDIN and STDOUT as /dev/null to protect channel from non-protocol
 * data. STDERR uses a separate channel and needs not to be changed; keep it
 * for debugging.
 *
 * FD 3 and 4 are used to not mix with STDIN=0, STDOUT=1 and STDERR=2.
 * Don't use dup() after closing previous FDs with close()!
 */
void init_server_pipes ( void )
{
	send_fd = move_fd(STDOUT_FILENO, 3);
	if ( debug_level )
		debug_printf ( "to_client fd=%d\n", send_fd );
	open_dev_null(STDOUT_FILENO, O_WRONLY);

	recv_fd = move_fd(STDIN_FILENO, 4);
	add_read_fd ( recv_fd, connection_read_handler );
	if ( debug_level )
		debug_printf ( "from_client fd=%d\n", recv_fd );
	open_dev_null(STDIN_FILENO, O_RDONLY);

	return;
}

/* pipes should be closed in remove_process() (process.c) */
void client_connection_exit_handler ( void )
{
	rsh_pid = -1;
	debug_printf ( "connection to server died\n" );
	should_exit = 0;
	return;
}

void connect_to_server ( void )
{
	char *argv[20];
	char **p = argv;
	char * session_rsh, * session_rsh_args, * session_server, * my_server;
	int to_fd, from_fd;
	char * ssh_debug = "-v"; /* XXX: this doesn't work with krsh */
	char * krsh_debug = "-d"; /* XXX: this doesn't work with krsh */
	char * server_debug = "-d2";
	char * krsh_forward = "-f";

	extern char * server_host;
	extern char * server_prog;

	session_rsh = getenv ( "SESSION_RSH" );
	if ( !session_rsh ) session_rsh = "rsh";
	session_rsh_args = getenv ( "SESSION_RSH_ARGS" );

	session_server = server_prog;
	if ( !session_server ) session_server = getenv ( "SESSION_SERVER" );
	if ( !session_server ) session_server = "univention-session-server";
	my_server = server_host;
	if ( !my_server ) my_server = getenv ( "SESSION_HOST" );
	if ( !my_server ) fatal_error ( "no server\n" );

	*p++ = session_rsh;
	if ( session_rsh_args ) {
		char * start = session_rsh_args;
		char * walk = session_rsh_args;

		while ( *walk != '\0' ) {
			while ( *walk != '\0' && *walk != ' ' ) walk++;
			if ( *walk == '\0' ) break;
			*walk = '\0';
			*p++ = start;
			start = walk + 1;
			walk++;
		}
		*p++ = start;
	}

	if ( strncmp( "krsh",session_rsh, 4)==0) {
		*p++ = krsh_forward;
		if ( debug_level > 1 ) {
			*p++ = krsh_debug;
		}
	} else if ( debug_level > 1 ) {
		*p++ = ssh_debug;
	}
	*p++ = my_server;
	*p++ = session_server;
	if ( debug_level )
		*p++ = server_debug;
	*p++ = NULL;

	if (debug_level)
	{
		int i;
		debug_printf ( "starting server: ");
		for (i = 0; argv[i]; i++)
			fprintf ( stderr, "%s ", argv[i]);
		fprintf ( stderr, "\n" );
	}

	rsh_pid = start_piped ( argv, &to_fd, &from_fd, client_connection_exit_handler );

	send_fd = to_fd;
	recv_fd = from_fd;
	add_read_fd ( recv_fd, connection_read_handler );

	if ( debug_level )
		debug_printf ( "pipes: to: %d from: %d\n", send_fd, recv_fd );

}
