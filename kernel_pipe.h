#ifndef KERNEL_PIPE_H
#define KERNEL_PIPE_H

#include "tinyos.h"

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
  uint32_t available_space;

	CondVar hasSpace;
	CondVar hasData;

	pipe_t* pipe;

	uint32_t reader_closed;
	uint32_t writer_closed;

} PipeCB;

int pipe_read (void* this, char *buf, unsigned int size);

int pipe_write (void* this, const char* buf, unsigned int size);

int reader_close (void* this);

int writer_close (void* this);

PipeCB* get_pipe();

#endif