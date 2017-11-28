
#include "tinyos.h"
#include "kernel_dev.h"

//@TODO: Should these always be 64 bit?
#define Kilobytes(Value) ((Value)*1024)
#define Megabytes(Value) (Kilobytes(Value)*1024)
// #define Gigabytes(Value) (Megabytes(Value)*1024)
// #define Terabytes(Value) (Gigabytes(Value)*1024)

typedef struct pipe_control_block {
	int8_t buffer[Kilobytes(16)];
} PipeCB;

int read (void* this, char *buf, unsigned int size)
{
	return -1;
}

int reader_close (void* this)
{
	return -1;
}

int write (void* this, const char* buf, unsigned int size)
{
	return -1;
}

int writer_close (void* this)
{
	return -1;
}

file_ops reader_ops = {
  .Open = NULL,
  .Read = read,
  .Write = NULL,
  .Close = reader_close
};

file_ops writer_ops = {
  .Open = NULL,
  .Read = NULL,
  .Write = write,
  .Close = writer_close
};

int sys_Pipe(pipe_t* pipe)
{
	return -1;
}