#ifndef __KERNEL_PROC_H
#define __KERNEL_PROC_H

/**
  @file kernel_proc.h
  @brief The process table and process management.

  @defgroup proc Processes
  @ingroup kernel
  @brief The process table and process management.

  This file defines the PCB structure and basic helpers for
  process access.

  @{
*/ 

#include "tinyos.h"
#include "kernel_sched.h"

/**
  @brief PID state

  A PID can be either free (no process is using it), ALIVE (some running process is
  using it), or ZOMBIE (a zombie process is using it).
  */
typedef enum pid_state_e {
  FREE,   /**< The PID is free and available */
  ALIVE,  /**< The PID is given to a process */
  ZOMBIE  /**< The PID is held by a zombie */
} pid_state;

/**
  @brief Process Control Block.

  This structure holds all information pertaining to a process.
 */
typedef struct process_control_block {
  pid_state  pstate;      /**< The pid state for this PCB */

  PCB* parent;            /**< Parent's pcb. */
  
  int exitval;            /**< The exit value */

  PTCB* main_thread;      /**< main thread*/
  
  rlnode children_list;   /**< List of children */
  rlnode exited_list;     /**< List of exited children */

  rlnode children_node;   /**< Intrusive node for @c children_list */
  rlnode exited_node;     /**< Intrusive node for @c exited_list */
  CondVar child_exit;     /**< Condition variable for @c WaitChild */

  FCB* FIDT[MAX_FILEID];  /**< The fileid table of the process */

  rlnode ptcb_list;       /**< List of PTCBs */
  uint64_t thread_count;       /**< Total number of threads. */
  Mutex thread_mx;

} PCB;

/**
  @brief Initialize the process table.

  This function is called during kernel initialization, to initialize
  any data structures related to process creation.
*/
void initialize_processes();

/**
  @brief Get the PCB for a PID.

  This function will return a pointer to the PCB of 
  the process with a given PID. If the PID does not
  correspond to a process, the function returns @c NULL.

  @param pid the pid of the process 
  @returns A pointer to the PCB of the process, or NULL.
*/
PCB* get_pcb(Pid_t pid);

/**
  @brief Get the PID of a PCB.

  This function will return the PID of the process 
  whose PCB is pointed at by @c pcb. If the pcb does not
  correspond to a process, the function returns @c NOPROC.

  @param pcb the pcb of the process 
  @returns the PID of the process, or NOPROC.
*/
Pid_t get_pid(PCB* pcb);

/**
  @brief Process Thread Control Block.
 */
typedef struct  p_thread_control_block
{
  PCB* owner_pcb;         /**< Owner PCB*/
  rlnode pthread;         /**< Node for intrusive list*/
  
  TCB* thread;
  int exitval;            /**< The exit value */

  CondVar waiting;        /**< Condition variable to wake sleeping threads so that they can be joined*/
  CondVar thread_join;    /**< Condition variable for @c ThreadJoin */
  int waiting_threads;    /**< Number of threads waiting on this thread*/
  int detached;           /**< If = 0 then thread is joinable */

  Task main_task;         /**< The thread's function */
  int argl;               /**< The thread's argument length */
  void* args;             /**< The thread's argument string */

}PTCB;


/**
  @brief Acquire PTCB.

  This function returns a PTCB struct with its members initialized and 
  pushes its rlnode in PCB's ptcb_list 
*/
PTCB* Create_PTCB(PCB* pcb);

/* ------------------------------ Open Info ------------------------------ */

/**
  @brief Info Control Block.

  This struct has all the date needed for OpenInfo()/sysinfo
  and acts as the streamobj for the associated stream.
 */
typedef struct info_control_block
{
  procinfo * info_table;  /**< A dynamic array with all the procinfo structs that were created during OpenInfo()*/
  uint32_t index;         /**< The index of next element to be drawn from array*/
} InfoCB;

/** @} */

#endif
