#ifndef KERNEL_PIPE_H
#define KERNEL_PIPE_H

//@TODO: Should these always be 64 bit?
#define Kilobytes(Value) ((Value)*1024)
#define Megabytes(Value) (Kilobytes(Value)*1024)
#define Gigabytes(Value) (Megabytes(Value)*1024)
#define Terabytes(Value) (Gigabytes(Value)*1024)

// Fancy macro we totally came up with.
#define ArrayCount(Array) (sizeof(Array)/sizeof((Array)[0]))

// ring buffer size for pipes
#define BUFF_SIZE Kilobytes(16)

/**
  @brief Pipe Control Block.

  This structure holds all information pertaining to a pipe.
 */

typedef struct pipe_control_block {
	int8_t buffer[BUFF_SIZE];					/**< Ring buffer for pipes*/
	uint32_t write_p;									/**< Write pointer*/
	uint32_t read_p;									/**< Read pointer*/
  uint32_t available_space;					/**< Space available in buffer*/

	CondVar hasSpace;									/**< CondVar that is woken up when there is space in buffer*/
	CondVar hasData;									/**< CondVar that is woken up when there is data in buffer*/

	pipe_t* pipe;											/**< The pipe_t we belong to*/

	uint16_t reader_closed;						/**< Flag for whether the reader was closed*/
	uint16_t writer_closed;						/**< Flag for whether the writer was closed*/

} PipeCB;

/**
  @brief Read from pipe.

	This function will try and read from a pipe.

  @param this pointer to PipeCB 
  @param buf pointer to buffer to read to
  @param size the size of the buffer
  @returns the number of bytes copied, 0 if we have reached EOF, or -1, indicating some error.
*/
int pipe_read (void* this, char *buf, unsigned int size);

/**
  @brief Write to pipe.

	This function will try and write to a pipe.

  @param this pointer to PipeCB 
  @param buf pointer to buffer to write from
  @param size the size of the buffer
  @returns the number of bytes copied or -1, indicating some error.
*/
int pipe_write (void* this, const char* buf, unsigned int size);

/**
  @brief Close a pipe from the reader side.

	This function will try to close a pipe from the
	reader side.

  @param this pointer to PipeCB 
  @returns 0 on success or -1 on failure.
*/
int reader_close (void* this);

/**
  @brief Close a pipe from the writer side.

	This function will try to close a pipe from the
	writer side.

  @param this pointer to PipeCB 
  @returns 0 on success or -1 on failure.
*/
int writer_close (void* this);

/**
  @brief Allocate and initialize a PipeCB.

	This function will try and allocate space for
	a PipeCB and initialize it.

  @returns valid pointer on success, NULL on failure.
*/
PipeCB* get_pipe();

#endif