
#include "tinyos.h"
#include "util.h"
#include "kernel_streams.h"
#include "kernel_pipe.h"
#include "kernel_cc.h"

// Constant to use with TimerDuration.
#define MILLISEC(value) (1000*(value))

typedef enum {
	UNBOUND,
	LISTENER,
	PEER
} Socket_type;

typedef union socket 
{
	struct  //Listener
	{
		CondVar reqs_cv;
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
	SCB* socket;
	CondVar conn_cv;
	int accepted;
	rlnode node;
} Conn_req;



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
					return pipe_write(scb->socket.send, buf, size);	 // How deep does the Rabbit hole go?
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
		if (scb->type == LISTENER)
		{
			scb->refcount = 0;
			Cond_Broadcast(&scb->socket.reqs_cv);
			yield(SCHED_USER);
			if (PortMap[scb->port] == scb)
			{
				PortMap[scb->port] = NULL;
			}
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

// create pipe function for sockets
// must be called with peer initialized
void create_pipe(Socket_t* socket)
{

	PipeCB * receive = get_pipe();
	
 	PipeCB * send = get_pipe();

  socket->send = send;
	socket->receive = receive;

  socket->peer->send = receive;
  socket->peer->receive = send;
}

Fid_t sys_Socket(port_t port)
{
	FCB* fcb;
	Fid_t fid;

	if (port < 0 || port > MAX_PORT)
		return NOFILE;

	if (!FCB_reserve(1, &fid, &fcb))
		return NOFILE;

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
	if (sock >= 0 && sock <= MAX_FILEID-1)
	{
		FCB* fcb = get_fcb(sock);
		if (fcb)
		{
			if (fcb->streamfunc == &socket_ops)
			{
				SCB* scb = (SCB*)fcb->streamobj;
				if (scb->port)
				{
					if (!PortMap[scb->port])
					{
						if (!scb->refcount)
						{
							scb->socket.reqs_cv = COND_INIT;
							rlnode_init(&scb->socket.req_queue, NULL);
							scb->refcount++;
							scb->type = LISTENER;
							
							PortMap[scb->port] = scb;

							return 0;
						}
					}
				}
			}
		}
	}

	return -1;	
}


Fid_t sys_Accept(Fid_t lsock)
{	

	/*
	Initializations and required checks.	
	*/
	
	if (lsock < 0 || lsock > MAX_FILEID-1)
		return NOFILE;

	FCB* fcb = get_fcb(lsock);
	if (!fcb)
		return NOFILE;
	
	if (fcb->streamfunc != &socket_ops)
		return NOFILE;

	SCB* l_scb = (SCB*)fcb->streamobj;

	if (l_scb->type != LISTENER || !PortMap[l_scb->port])
		return NOFILE;

	rlnode* req_queue = & l_scb->socket.req_queue;

	/*
	Check if there is a connection request already, otherwise wait.
	*/
	if(is_rlist_empty(req_queue))
	{
   //  kernel_timedwait(
			// /*CondVar*/ & l_scb->socket.reqs_cv , 
			// /* cause */ SCHED_USER, 
			// /*TimeDur*/ MILLISEC(2500)//*1000000
   //  );
		kernel_wait(& l_scb->socket.reqs_cv,SCHED_PIPE);
		if (!l_scb->refcount)
		{
			return NOFILE;
		}
	}	

	// if (is_rlist_empty(req_queue))
	// {
	// 	// something went wrong
	// 	return NOFILE;
	// }

	/* 
	Creating the peer sockets and their pipes and connecting them.
	*/

	Conn_req* conn_struct = rlist_pop_front(req_queue)->conn_req;

	SCB* client_peer = conn_struct->socket;
	client_peer->refcount = 1;

	// 2. Create server socket.
	Fid_t server_fid = sys_Socket(l_scb->port);

	if (server_fid == NOFILE)
  {
    Cond_Signal(&conn_struct->conn_cv);
    return NOFILE;
  }
		
	fcb = get_fcb(server_fid);
	SCB* server_peer = (SCB*)fcb->streamobj;

	server_peer->refcount = 1;

	// 3. Change the 2 sockets type to Peers.
	server_peer->type = PEER;
	client_peer->type = PEER;

	// 4. Connect peers with each other.
	server_peer->socket.peer = &client_peer->socket;
	client_peer->socket.peer = &server_peer->socket;
	
	// 5. Create Pipes.

	create_pipe(&server_peer->socket);

	// 6. Wake up client socket (from CondVar inside Conn_req struct).
	conn_struct->accepted = 1;
	Cond_Signal(&conn_struct->conn_cv);

	// 7. Return Server's peer.

	return server_fid;
}


int sys_Connect(Fid_t sock, port_t port, timeout_t timeout)
{	

	if (sock < 0 || sock > MAX_FILEID-1)
		// wake any potentialy waiting threads
		return -1;

	if (port < 0 || port > MAX_PORT)
		return -1;

	// 1. Find if Listener exists in PortMap (public var)

	SCB* lsocket = PortMap[port];
	if (!lsocket)
		return -1;

	if(!(lsocket->type == LISTENER)){
		//@TODO remove   ...or not.
		fprintf(stderr, "No Listener socket at port: %d\n",  (int)port );
		return -1;
	}
	
	// 2. Create and fill connection struct

	FCB* fcb = get_fcb(sock);
	if (!fcb)
		return -1;

	SCB* scb = (SCB*)fcb->streamobj;
	if (scb->type != UNBOUND )
		return -1;

	/* Connection Request Struct INIT. */
	Conn_req* conn_struct = (Conn_req*)malloc(sizeof(Conn_req));

	// fix me
	
	conn_struct->socket = scb;
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
	while(!conn_struct->accepted)
	{
		int flag = kernel_timedwait(
			/*CondVar*/ &conn_struct->conn_cv , 
			/* cause */ SCHED_USER, 
			/*TimeDur*/ MILLISEC(timeout)//*1000000
		);
		if(flag && !conn_struct->accepted)
			return -1;

		if (!flag)
			return -1;
	}

	// int flag;
	// do
	// {
	// 	/* Returns 1 if signalled, 0 otherwise.*/
	// 	flag = 
	// 	//@TODO remove
	// 	if(flag == 0)
	// 		fprintf(stderr, "%s\n", "Server didn't respond.");	

	// }while(flag == 0);

	// kernel_wait(&conn_struct->conn_cv, SCHED_USER);

	return 0;
}


int sys_ShutDown(Fid_t sock, shutdown_mode how)
{
	if (sock >= 0 && sock <= MAX_FILEID-1)
	{
		if (how)
		{
			FCB* fcb = get_fcb(sock);
			if (fcb)
			{
				if (fcb->streamfunc == &socket_ops)
				{
					SCB* scb = (SCB*)fcb->streamobj;
					if (scb->type == PEER)
					{
						if (scb->socket.peer)
						{
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
					}
				}
			}
		}
	}

	return -1;
}


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


// ========================================


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