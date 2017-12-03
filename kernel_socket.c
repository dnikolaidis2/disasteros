
#include "tinyos.h"
#include "util.h"
#include "kernel_streams.h"
#include "kernel_pipe.h"

typedef enum {
	UNBOUND,
	LISTENER,
	PEER
} Socket_type;

typedef struct unbound
{
	// padding?
	rlnode node;	// but why?
}Unbound;

typedef struct listener
{
	CondVar reqs;
	rlnode req_queue;
}Listener;

typedef struct peer
{
	struct peer* peer;
	PipeCB send;
	PipeCB receive;
}Peer;

typedef struct socket_control_block {
	int refcount;
	FCB* fcb;
	port_t port;
	Socket_type type;

	union
	{
		// BUT I WANTED MY STRUCTS TO BE ANONYMOUS ಥ_ಥ
		Unbound unbound;
		Listener listener;
		Peer peer;
	};
} SCB;

typedef struct socket_connection_block {
	union {
		Unbound* unbound;
		Peer* peer;
	};

	rlnode node;
	CondVar conn;
	int accepted;
} ConnBlock;

static SCB* PortMap [MAX_PORT] = {0};

int socket_read(void* this, char *buf, unsigned int size)
{
	return -1;
}
int socket_write(void* this, const char* buf, unsigned int size)
{
	return -1;
}
int socket_close(void* this)
{
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

	return NOFILE;
}

int sys_Listen(Fid_t sock)
{
	return -1;
}


Fid_t sys_Accept(Fid_t lsock)
{
	return NOFILE;
}


int sys_Connect(Fid_t sock, port_t port, timeout_t timeout)
{
	return -1;
}


int sys_ShutDown(Fid_t sock, shutdown_mode how)
{
	return -1;
}

