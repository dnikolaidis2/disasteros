
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
	uint32_t writer_p;
	uint32_t reader_p;

	CondVar hasSpace;
	CondVar hasData;

	pipe_t* pipe;

} PipeCB;

int pipe_read (void* this, char *buf, unsigned int size)
{
	PipeCB* pipe = (PipeCB*) this;
	unsigned int bytes_read = 0;

	if (pipe->reader_p == pipe->writer_p)
	{
		kernel_wait(&pipe->hasData, SCHED_PIPE);
	}
	
	if ((pipe->reader_p + size) % BUFF_SIZE <= pipe->writer_p)
	{
		if (pipe->writer_p < pipe->reader_p && pipe->reader_p + size > BUFF_SIZE)
		{
			memcpy(buf, &pipe->buffer[pipe->reader_p], BUFF_SIZE - pipe->reader_p);
			memcpy(buf + (BUFF_SIZE - pipe->reader_p), pipe->buffer, (pipe->reader_p + size) % BUFF_SIZE);
		}
		else
		{
			memcpy(buf, &pipe->buffer[pipe->reader_p], size);
		}
		bytes_read += size;
	}
	else
	{
		bytes_read = size - (pipe->writer_p - (pipe->reader_p + size) % BUFF_SIZE);
		if (pipe->writer_p < pipe->reader_p && pipe->reader_p + bytes_read > BUFF_SIZE)
		{
			memcpy(buf, &pipe->buffer[pipe->reader_p], BUFF_SIZE - pipe->reader_p);
			memcpy(buf + (BUFF_SIZE - pipe->reader_p), pipe->buffer, (pipe->reader_p + bytes_read) % BUFF_SIZE);
		}
		else
		{
			memcpy(buf, &pipe->buffer[pipe->reader_p], bytes_read);
		}
	}

	if (bytes_read)
	{
		pipe->reader_p = (pipe->reader_p + bytes_read) % BUFF_SIZE;
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
  return -1;
}

int pipe_write (void* this, const char* buf, unsigned int size)
{	
	PipeCB* pipe = (PipeCB*) this;
	uint32_t write_p = pipe->writer_p;
	uint32_t read_p = pipe->reader_p;
	uint32_t buff_size = BUFF_SIZE;

	// We assume the Pipe's buffer is full if all but one cell is used.
		if(read_p == ( write_p + 1 ) % (buff_size))
	{			
		// Will sleep until buffer isn't empty.
		kernel_wait(& pipe->hasSpace, SCHED_PIPE);
	}
	else
	{	
		// Gets available space on Pipe's buffer.
		int available_space = write_p - read_p;
		if(available_space == 0)
		{
			available_space = buff_size - 1;
		}		

		if (available_space >= size)
		{
			for (int i = 0; i < size; ++i)
			{
				pipe->buffer[write_p] = buf[i];

				// Increments write pointer by 1, cycling through the array;
				write_p = (write_p + 1) %(buff_size);
			}
			
			// Wake up any sleeping readers.
			Cond_Broadcast(&pipe->hasData);
			return size;
		}
		else;
		{
			for (int i = 0; i < available_space; ++i)
			{
				pipe->buffer[write_p] = buf[i];

				// Increments write pointer by 1, cycling through the array;
				write_p = (write_p + 1) %(buff_size);
			}
			
			// Wake up any sleeping readers.
			Cond_Broadcast(&pipe->hasData);
			return available_space;
		}
		

	}


	return -1;
}

int writer_close (void* this)
{
	return -1;
}

file_ops reader_ops = {
  .Open = NULL,
  .Read = pipe_read,
  .Write = NULL,
  .Close = reader_close
};

file_ops writer_ops = {
  .Open = NULL,
  .Read = NULL,
  .Write = pipe_write,
  .Close = writer_close
};

int sys_Pipe(pipe_t* pipe)
{
	FCB* files [2];
	Fid_t fids [2];

	if(!FCB_reserve(2, fids, files))
	{
		return -1;
	}

	pipe->read = fids[0];
	pipe->write = fids[1];

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