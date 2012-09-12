

PROGS	= client server client-run
SRCS 	= signals.c connection.c script.c command.c protocol.c \
		process.c debug.c pipe.c security.c alive.c select_loop.c \
		session.c command_socket.c command_socket.c

CL_SRCS		= client_main.c
SRV_SRCS	= server_main.c
CLR_SRCS	= client_run_main.c

LIB	= libcs.a
LIBS = -luniventionconfig -luniventiondebug

CFLAGS	= -O2 -Wall -g
CC	= gcc

all:	${PROGS}

OBJS		= $(SRCS:.c=.o)
CL_OBJS		= $(CL_SRCS:.c=.o)
SRV_OBJS	= $(SRV_SRCS:.c=.o)
CLR_OBJS	= $(CLR_SRCS:.c=.o)

${LIB}: ${OBJS}
	ar rcu ${LIB} ${OBJS}

client:	${LIB} ${CL_OBJS}
	$(CC) -o univention-session-client ${CL_OBJS} ${LIB} $(LIBS) $(LDFLAGS)

server:	${LIB} ${SRV_OBJS}
	$(CC) -o univention-session-server ${SRV_OBJS} ${LIB} $(LIBS) $(LDFLAGS)

client-run:	${LIB} ${CLR_OBJS}
	$(CC) -o univention-session-client-run ${CLR_OBJS} ${LIB} -lpthread $(LIBS) $(LDFLAGS)

clean:
	${RM} ${OBJS} ${OBJS} ${SRV_OBJS} ${CL_OBJS} ${CLR_OBJS} *~ ${PROGS} ${LIB}
