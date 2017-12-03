
#include "tinyos.h"
#include "kernel_pipe.h"
#include "kernel_cc.h"
#include "kernel_streams.h"
#include "kernel_dev.h"

int pipe_read (void* this, char *buf, unsigned int size)
{
	PipeCB* pipe = (PipeCB*) this;
	
	unsigned int bytes_read = 0;

	while(pipe->read_p == pipe->write_p && pipe->available_space == BUFF_SIZE)
	{
		if (pipe->writer_closed)
		{
			return 0;
		}

		kernel_wait(&pipe->hasData, SCHED_PIPE);
	}

  uint32_t read_p = pipe->read_p;

	for (int i = 0; i < (BUFF_SIZE - pipe->available_space); ++i)
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
    pipe->available_space += bytes_read;
		Cond_Broadcast(&pipe->hasSpace);
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
	if (pipe->reader_closed && pipe->writer_closed)
	{
		free(pipe);
	}
  return -1;
}

int pipe_write (void* this, const char* buf, unsigned int size)
{	
	PipeCB* pipe = (PipeCB*) this;
	
	int bytes_writen = 0;

	if (pipe->reader_closed)
	{	
		return -1;
	}
	
	while(pipe->read_p == pipe->write_p && !pipe->available_space)
	{			
		kernel_wait(& pipe->hasSpace, SCHED_PIPE);
	}

  uint32_t write_p = pipe->write_p;

	// Gets available space on Pipe's buffer.

	for (int i = 0; i < pipe->available_space; ++i)
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
    pipe->available_space -= bytes_writen;
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
	if (pipe->reader_closed && pipe->writer_closed)
	{
		free(pipe);
	}
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
  pcb->available_space = BUFF_SIZE;
	pcb->pipe = pipe;

	files[0]->streamobj = pcb;
	files[0]->streamfunc = &reader_ops;

	files[1]->streamobj = pcb;
	files[1]->streamfunc = &writer_ops;

	return 0;
}