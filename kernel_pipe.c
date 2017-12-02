
#include "tinyos.h"
#include "kernel_streams.h"
#include "kernel_dev.h"
#include "kernel_cc.h"

//@TODO: Should these always be 64 bit?
#define Kilobytes(Value) ((Value)*1024)
#define Megabytes(Value) (Kilobytes(Value)*1024)
// #define Gigabytes(Value) (Megabytes(Value)*1024)
// #define Terabytes(Value) (Gigabytes(Value)*1024)

// Fancy macro we totally came up with.
#define ArrayCount(Array) (sizeof(Array)/sizeof((Array)[0]))

#define BUFF_SIZE Kilobytes(16)

typedef struct pipe_control_block {
	int8_t buffer[BUFF_SIZE];
	uint32_t write_p;
	uint32_t read_p;

	CondVar hasSpace;
	CondVar hasData;

	pipe_t* pipe;

	uint32_t reader_closed;
	uint32_t writer_closed;

} PipeCB;

int pipe_read (void* this, char *buf, unsigned int size)
{
	PipeCB* pipe = (PipeCB*) this;
	uint32_t write_p = pipe->write_p;
	uint32_t read_p = pipe->read_p;
	
	unsigned int bytes_read = 0;

	if (read_p == write_p)
	{
		if (pipe->writer_closed)
		{
			return 0;
		}

		kernel_wait(&pipe->hasData, SCHED_PIPE);
		write_p = pipe->write_p;
		read_p = pipe->read_p;
	}

	int available_data = (BUFF_SIZE - read_p + write_p) % BUFF_SIZE;

	for (int i = 0; i < available_data; ++i)
	{
		buf[i] = pipe->buffer[read_p];
		bytes_read++;

		read_p = (read_p + 1) %(BUFF_SIZE);

		if (bytes_read == size)
		{
			break;
		}
	}

	if (bytes_read)
	{
		pipe->read_p = read_p;
		Cond_Signal(&pipe->hasSpace);
		return bytes_read;
	}
	else
	{
		return -1;
	}
}
	
int reader_close (void* this)
{
	PipeCB* pipe = (PipeCB *) this;
	pipe->reader_closed = 1;
  return -1;
}

int pipe_write (void* this, const char* buf, unsigned int size)
{	
	PipeCB* pipe = (PipeCB*) this;
	
	uint32_t write_p = pipe->write_p;
	uint32_t read_p = pipe->read_p;
	int bytes_writen = 0;

	if (pipe->reader_closed)
	{	
		return -1;
	}
	
	// We assume the Pipe's buffer is full if all but one cell is used.
	if(read_p == ( write_p + 1 ) % BUFF_SIZE)
	{			
		// Will sleep until buffer isn't empty.
		kernel_wait(& pipe->hasSpace, SCHED_PIPE);
		write_p = pipe->write_p;
		read_p = pipe->read_p;
	}

	// Gets available space on Pipe's buffer.
	int available_space = (write_p - read_p + BUFF_SIZE) % BUFF_SIZE;

	if(available_space == 0)
	{
		available_space = BUFF_SIZE - 1;
	}

	if((available_space + write_p) % BUFF_SIZE == read_p)
	{
		available_space--;
	}

	for (int i = 0; i < available_space; ++i)
	{
		pipe->buffer[write_p] = buf[i];
		bytes_writen++;

		// Increments write pointer by 1, cycling through the array;
		write_p = (write_p + 1) %(BUFF_SIZE);

		if (bytes_writen == size)
		{
			break;
		}
	}

	if (bytes_writen > 0)
	{
		// Update write pointer;
		pipe->write_p = write_p;
		// Wake up any sleeping readers.
		Cond_Broadcast(&pipe->hasData);
		return bytes_writen;
	}
	else
	{
		return -1;
	}	
}

int writer_close (void* this)
{
	PipeCB* pipe = (PipeCB *) this;
	pipe->writer_closed = 1;
	return -1;
}

static file_ops reader_ops = {
  .Open = NULL,
  .Read = pipe_read,
  .Write = NULL,
  .Close = reader_close
};

static file_ops writer_ops = {
  .Open = NULL,
  .Read = NULL,
  .Write = pipe_write,
  .Close = writer_close
};

int sys_Pipe(pipe_t* pipe)
{
	FCB* files [2];

	if(!FCB_reserve(2, (Fid_t*)pipe, files))
	{
		return -1;
	}

	PipeCB * pcb = (PipeCB *)malloc(sizeof(PipeCB));
	memset(pcb, 0, sizeof(PipeCB));

	pcb->hasSpace = COND_INIT;
	pcb->hasData = COND_INIT;
	pcb->pipe = pipe;

	files[0]->streamobj = pcb;
	files[0]->streamfunc = &reader_ops;

	files[1]->streamobj = pcb;
	files[1]->streamfunc = &writer_ops;

	return 0;
}