
#include "tinyos.h"
#include "util.h"
#include "kernel_streams.h"
#include "kernel_pipe.h"
#include "kernel_cc.h"

// Constant to use with TimerDuration.
#define MILLISEC 1000

typedef enum {
	UNBOUND,
	LISTENER,
	PEER
} Socket_type;

typedef union socket 
{
	struct  //Listener
	{
		CondVar reqs_cv; //@TODO Changed name from reqs to reqs_cv
		rlnode req_queue;
	};
	struct  //peer
	{
		union socket* peer;
		PipeCB* send;
		PipeCB* receive;
	};
}Socket_t;
// damn naming conflicts

/*
typedef struct socket
{
	Socket_type type;
	union 
	{
		struct  //Listener
		{
			CondVar reqs_cv;
			rlnode req_queue;
		};
		struct  //peer
		{
			struct socket* peer;
			PipeCB* send;
			PipeCB* receive;
		};
	};
	rlnode node;
}Socket_t;
*/


typedef struct socket_control_block
{
	int refcount;
	port_t port;
	Socket_type type;

	Socket_t socket;
	// alt
	// Socket_t* listener;
	// rlnode socket_list?
	// add node to socket_t
	// make Portmap array of SCB's
	// FCB*
}SCB;

typedef struct socket_connection_request
{
	Socket_t* socket;
	CondVar conn_cv; //@TODO Changed name from conn to conn_cv
	int accepted;
	rlnode node;
} Conn_req;

// typedef struct unbound
// {
	// padding?
	// rlnode node;	// but why?
// }Unbound;

// typedef struct listener
// {
	// CondVar reqs;
	// rlnode req_queue;
// }Listener;
// 
// typedef struct peer
// {
	// struct peer* peer;
	// PipeCB send;
	// PipeCB receive;
// }Peer;

// typedef struct socket_control_block 
// {
// 	int refcount;
// 	FCB* fcb;
// 	port_t port;
// 	Socket_type type;

// 	union
// 	{
		// BUT I WANTED MY STRUCTS TO BE ANONYMOUS ಥ_ಥ
// 		Unbound unbound;
// 		Listener listener;
// 		Peer peer;
// 	};
// } SCB;

// typedef struct socket_connection_block 
// {
// 	union 
// 	{
// 		Unbound* unbound;
// 		Peer* peer;
// 	};

// 	rlnode node;
// 	CondVar conn;
// 	int accepted;
// } ConnBlock;

#define PORT SCB*

// maybe better full caps name since public static?
static PORT PortMap [MAX_PORT+1] = {0};
// static uint16_t bound_ports = 0;

int socket_read(void* this, char *buf, unsigned int size)
{
	SCB* scb = (SCB*)this;
	if (scb)
	{	
		if (scb->type == PEER)
		{
			if (scb->socket.peer)
			{
				if (scb->socket.receive)
				{
					return pipe_read(scb->socket.receive, buf, size);
				}
			}
		}
	}

	return -1;
}
int socket_write(void* this, const char* buf, unsigned int size)
{
	SCB* scb = (SCB*)this;
	if (scb)
	{	
		if (scb->type == PEER)
		{
			if (scb->socket.peer)
			{
				if (scb->socket.send)
				{
					return pipe_write(scb->socket.send, buf, size);
				}
			}
		}
	}

	return -1;
}
int socket_close(void* this)
{
	SCB* scb = (SCB*)this;
	if (scb->refcount <= 1)
	{
		if (PortMap[scb->port] == scb)
		{
			PortMap[scb->port] = NULL;
		}
		free(this);
		return 0;
	}
	return -1;
}

static file_ops socket_ops = {
  .Open = NULL,
  .Read = socket_read,
  .Write = socket_write,
  .Close = socket_close
};

Fid_t sys_Socket(port_t port)
{
	FCB* fcb;
	Fid_t fid;

	if (port < 0 || port > MAX_PORT)
	{
		return NOFILE;
	}

	if (!FCB_reserve(1, &fid, &fcb))
	{
		return NOFILE;
	}

	SCB* scb = (SCB*)malloc(sizeof(SCB));
	memset(scb, 0, sizeof(SCB));
	
	if (port)
	{
		// scb->refcount++;
		scb->port = port;
	}

	fcb->streamobj = scb;
	fcb->streamfunc = &socket_ops;

	return fid;
}

int sys_Listen(Fid_t sock)
{
	if (sock < 0 || sock > MAX_FILEID-1)
	{
		return -1;
	}

	FCB* fcb = get_fcb(sock);
	if (!fcb)
	{
		return -1;
	}

	if (fcb->streamfunc != &socket_ops)
	{
		return -1;
	}

	SCB* scb = (SCB*)fcb->streamobj;
	if (!scb->port)
	{
		return -1;
	}

	if (PortMap[scb->port])
	{
		return -1;
	}

	if (scb->refcount)
	{
		return -1;
	}

	scb->socket.reqs_cv = COND_INIT;
	rlnode_init(&scb->socket.req_queue, NULL);
	scb->refcount++;
	
	PortMap[scb->port] = scb;

	return 0;
}


Fid_t sys_Accept(Fid_t lsock)
{	
	// 0.9 do checks
	// 1. Wait for request

	// If no requests, sleep.

	// 2. Make 1 socket.
	// 3. Change the 2 sockets type to Peers.
	// 4. Create Pipes.
	// 5. Wake up client socket (from CondVar inside Conn_req struct).
	// 6. Return Server's peer.
	// 7. Profit!
	return NOFILE;
}


int sys_Connect(Fid_t sock, port_t port, timeout_t timeout)
{	

	// 1. Find if Listener exists in PortMap (public var)

	SCB* lsocket = PortMap[port];

	if(!(lsocket->type == LISTENER)){
		//@TODO remove   ...or not.
		fprintf(stderr, "No Listener socket at port: %d\n",  (int)port );
		return -1;
	}
	
	// 2. Create and fill connection struct

	/* Connection Request Struct INIT. */
	Conn_req* conn_struct = (Conn_req*)malloc(sizeof(Conn_req));

	// fix me
	FCB* fcb = get_fcb(sock);
	SCB* scb = (SCB*)fcb->streamobj;
	conn_struct->socket = &scb->socket;
	conn_struct->conn_cv = COND_INIT;
	conn_struct->accepted = 0;
	rlnode_init(& conn_struct->node, conn_struct);

	// 3. Send req

	// not sure about this one either
	/* Prepare request queue. */
	rlist_push_back(& lsocket->socket.req_queue, &conn_struct->node);
	/* Wake up listener. */
	Cond_Signal(& lsocket->socket.reqs_cv);


	// 4. Sleep until having answer
	int flag;
	do
	{
		/* Returns 1 if signalled, 0 otherwise.*/
		flag = kernel_timedwait(
			/*CondVar*/ &conn_struct->conn_cv , 
			/* cause */ SCHED_PIPE , 
			/*TimeDur*/ 5000*MILLISEC
		);

		//@TODO remove
		if(flag == 0)
			fprintf(stderr, "%s\n", "Server didn't respond.");	

	}while(flag == 0);

	return 1;
}


int sys_ShutDown(Fid_t sock, shutdown_mode how)
{
	if (sock < 0 || sock > MAX_FILEID-1)
	{
		return -1;
	}

	if (!how)
	{
		return -1;
	}

	FCB* fcb = get_fcb(sock);
	if (!fcb)
	{
		// did we succed?
		return 0;
	}

	if (fcb->streamfunc != &socket_ops)
	{
		return -1;
	}

	SCB* scb = (SCB*)fcb->streamobj;
	if (scb->type != PEER)
	{
		return -1;
	}

	if (!scb->socket.peer)
	{
		return -1;
	}

	if (how == SHUTDOWN_READ || how == SHUTDOWN_BOTH)
	{
		if (scb->socket.receive)
		{
			reader_close(scb->socket.receive);
			scb->socket.receive = NULL;
			writer_close(scb->socket.peer->send);
			scb->socket.peer->send = NULL;
		}
	}
	
	if (how == SHUTDOWN_WRITE || how == SHUTDOWN_BOTH)
	{
		if (scb->socket.send)
		{
			writer_close(scb->socket.send);
			scb->socket.send = NULL;
			reader_close(scb->socket.peer->receive);
			scb->socket.peer->receive = NULL;
		}	
	}

	return 0;
}

