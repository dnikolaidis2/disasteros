
#include "tinyos.h"
#include "util.h"
#include "kernel_streams.h"
#include "kernel_pipe.h"
#include "kernel_cc.h"

/*
	Constant to use with TimerDuration. "value" is in microseconds. 
*/
#define MILLISEC(value) (1000*(value))

/*
	Socket type enum.
*/
typedef enum {
	UNBOUND,
	LISTENER,
	PEER
} Socket_type;

/*
	Union to use inside SCB, to differentiate Listeners from Peers.
*/
typedef union socket 
{
	struct  // Listener
	{
		CondVar reqs_cv;
		rlnode req_queue;
	};
	struct  // Peer
	{
		union socket* peer;
		PipeCB* send;
		PipeCB* receive;
	};
}Socket_t;


/**
	@brief Socket Control Block. 

	This structure holds all information pertaining	to a socket.
	Variable refcount currently not in use.
*/
typedef struct socket_control_block
{
	int refcount;			/* Commonly used for holding the amount of connections
											 associated with this socket*/
	port_t port;			/* The port this socket is associated with. */

	Socket_type type; /* The sockets type. (Socket_type enum) */
	Socket_t socket;	/* The socket's union holding data for either a 
											 a Listener or a Peer socket. */

	Fid_t fid;				/* Socket's fid. */
}SCB;

/*
	Structure that is responsible for handling connection requests.
*/
typedef struct socket_connection_request
{
	SCB* socket;			/* Socket (client) that tries to connect to port. */

	CondVar conn_cv;	/* Condition Variable client will wait on for Listener
											 to accept him. */

	int accepted;			/* Flag that client checks when woken up.
											 If 1: Accepted, else timed-out. */

	rlnode node;			/* rlNode needed in order to append on 
											 Listener's req_queue list.*/
} Conn_req;


#define PORT SCB*

static PORT PortMap [MAX_PORT+1] = {0};

/*
	file_ops Read();
*/
int socket_read(void* this, char *buf, unsigned int size)
{
	SCB* scb = (SCB*)this;

	if (scb)												// Check if Socket is NULL.
	{	
		if (scb->type == PEER)				// Check if Socket type is PEER
		{
			if (scb->socket.peer)				// Check if Peer Socket isn't NULL.
			{
				if (scb->socket.receive)	// Check if receive Pipe isn't NULL.
				{
					return pipe_read(scb->socket.receive, buf, size);	 // How deep does the Rabbit hole go?
				}
				else
				{
					return 0;	// EOF
				}
			}
		}
	}

	return -1;
}

/*
	file_ops Write();
*/
int socket_write(void* this, const char* buf, unsigned int size)
{
	SCB* scb = (SCB*)this;

	if (scb)										// Check if Socket is NULL.
	{	
		if (scb->type == PEER)		// Check if Socket type is PEER
		{
			if (scb->socket.peer)		// Check if Peer Socket isn't NULL.
			{
				if (scb->socket.send)	// Check if receive Pipe isn't NULL
				{
					return pipe_write(scb->socket.send, buf, size);
				}
			}
		}
	}

	return -1;
}

/*
	file_ops Close();
*/
int socket_close(void* this)
{
	SCB* scb = (SCB*)this;
	if (scb->refcount <= 1)
	{
		if (scb->type == LISTENER)
		{
			scb->refcount = 0;

			/*
				Wake up Listener.
			*/
			Cond_Broadcast(&scb->socket.reqs_cv);
			yield(SCHED_USER);

			/*
				Make sure PORT is NULL.
			*/
			if (PortMap[scb->port] == scb)
			{
				PortMap[scb->port] = NULL;
			}
		}
		else if (scb->type == PEER)
		{	
			/*
				If peer, close both sides (client and server socket).
			*/
			sys_ShutDown(scb->fid, SHUTDOWN_BOTH);
			scb->socket.peer->receive = NULL;
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

/*
	Create pipe function for sockets.
	Must be called with peer initialized beforehand.
*/
void create_pipe(Socket_t* socket)
{

	PipeCB * receive = get_pipe();
	
 	PipeCB * send = get_pipe();

  socket->send = send;
	socket->receive = receive;

  socket->peer->send = receive;
  socket->peer->receive = send;
}

/**
	@brief Creates UNBOUND socket for given port.

	Creates an initialized socket_type = UNBOUND socket
	for given port. Returns socket's fid_t.
*/
Fid_t sys_Socket(port_t port)
{
	FCB* fcb;
	Fid_t fid;

	if (port < 0 || port > MAX_PORT)
		return NOFILE;

	if (!FCB_reserve(1, &fid, &fcb))
		return NOFILE;

	SCB* scb = (SCB*)malloc(sizeof(SCB));
	if (!scb)
	{
		fprintf(stderr, "FATAL: Could not allocate enough memory\n");
		return NOFILE;
	}
	memset(scb, 0, sizeof(SCB));
	
	if (port)
	{
		// scb->refcount++;
		scb->port = port;
		scb->fid = fid;
	}

	fcb->streamobj = scb;
	fcb->streamfunc = &socket_ops;

	return fid;
}

/*
	Creates a Listener type socket, given an UNBOUND type one.

	Generally called from Server thread.

	Returns success or fail flag.
*/
int sys_Listen(Fid_t sock)
{
	if (sock >= 0 && sock <= MAX_FILEID-1)
	{	

		/*
			Find FCB from fid_t.
		*/
		FCB* fcb = get_fcb(sock);

		if (fcb)
		{
			if (fcb->streamfunc == &socket_ops)
			{
				/*
					Get socket control object from FCB.
				*/
				SCB* scb = (SCB*)fcb->streamobj;

				if (scb->port)
				{
					if (!PortMap[scb->port])
					{
						if (!scb->refcount)
						{

							/*
								Initialize Listener socket members.
							*/
							scb->socket.reqs_cv = COND_INIT;
							rlnode_init(&scb->socket.req_queue, NULL);
							scb->refcount++;
							scb->type = LISTENER;
							
							/*
								Bound Listener's scb to associated port.
							*/
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

/*
	Accept one pending request from req_queue or wait for one
	on listener's cond.variable.
	
	Generally called from Server thread.

	Return's accepted socket's fidt_t.
*/
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
		0.5 Check if there is a connection request already, otherwise wait.
	*/
	if(is_rlist_empty(req_queue))
	{
		kernel_wait(& l_scb->socket.reqs_cv,SCHED_PIPE);
		if (!l_scb->refcount)
		{
			return NOFILE;
		}
	}	

	
	// 1. Creating the peer sockets and their pipes and connecting them.
	Conn_req* conn_struct = rlist_pop_front(req_queue)->conn_req;

	SCB* client_peer = conn_struct->socket;
	client_peer->refcount = 1;

	// 2. Create server socket.
	Fid_t server_fid = sys_Socket(l_scb->port);

	if (server_fid == NOFILE)
  {
  	/*
			Wake up client with error flag (accepted = 0).
  	*/
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

/*
	Waits until either a connection is established on given port
	OR until timeout.
	NOTE: In this implementation: if timedout, retry until accepted. 

	Generally called from Client thread.

	Returns success or not flag.
*/
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
	if (!conn_struct)
	{
		fprintf(stderr, "FATAL: Could not allocate enough memory\n");
		return -1;
	}
	
	conn_struct->socket = scb;
	conn_struct->conn_cv = COND_INIT;
	conn_struct->accepted = 0;
	rlnode_init(& conn_struct->node, conn_struct);

	// 3. Send req
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
			/*TimeDur*/ MILLISEC(timeout)
		);
		if(flag && !conn_struct->accepted)
			return -1;

		if (!flag)
			return -1;
	}

	return 0;
}

/*
	Shuts down Reader OR Writer OR Both.
*/
int sys_ShutDown(Fid_t sock, shutdown_mode how)
{
	if (sock >= 0 && sock <= MAX_FILEID-1)
	{
		if (how)
		{
			/*
				Find FCB from fid_t.
			*/
			FCB* fcb = get_fcb(sock);

			if (fcb)
			{
				if (fcb->streamfunc == &socket_ops)
				{
					/*
						Get socket control object from FCB.
					*/
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
									scb->socket.peer->send = NULL;
								}
							}
							
							if (how == SHUTDOWN_WRITE || how == SHUTDOWN_BOTH)
							{
								if (scb->socket.send)
								{
									writer_close(scb->socket.send);
									scb->socket.send = NULL;
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