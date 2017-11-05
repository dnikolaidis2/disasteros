
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"

// @TODO do we need this? probably not
#ifdef MMAPPED_THREAD_MEM 

/*
  Use mmap to allocate a p_thread. A more detailed implementation can allocate a
  "sentinel page", and change access to PROT_NONE, so that a stack overflow
  is detected as seg.fault.
 */
void free_p_thread(void* ptr, size_t size)
{
  CHECK(munmap(ptr, size));
}

void* allocate_p_thread(size_t size)
{
  void* ptr = mmap(NULL, size, 
      PROT_READ|PROT_WRITE|PROT_EXEC,  
      MAP_ANONYMOUS  | MAP_PRIVATE 
      , -1,0);
  
  CHECK((ptr==MAP_FAILED)?-1:0);
  memset(ptr, 0, size);

  return ptr;
}
#else
/*
  Use malloc to allocate a p_thread. This is probably faster than  mmap, but cannot
  be made easily to 'detect' stack overflow.
 */
void free_p_thread(void* ptr, size_t size)
{
  free(ptr);
}

void* allocate_p_thread(size_t size)
{
  void* ptr = malloc(size);
  CHECK((ptr==NULL)?-1:0);
  memset(ptr, 0, size);
  return ptr;
}
#endif


  /**
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void* args)
{

  PTCB* ptcb = allocate_p_thread(sizeof(PTCB));         // Allocate memory
             
  ptcb->thread_join = COND_INIT;                          // Init CondVar
  CURTHREAD->owner_ptcb->main_task = task;
  CURTHREAD->owner_ptcb->argl = argl;
  CURTHREAD->owner_ptcb->args = args;

  rlnode_init(& ptcb->pthread, ptcb);                     // Init rlNode
  rlist_push_back(& CURPROC->ptcb_list, & ptcb->pthread); // Add PThread to parent PCB's list

  // Spawn thread
  if(task != NULL)
  {
    ptcb->thread = spawn_thread(CURPROC, start_thread_func);
    ptcb->thread->owner_ptcb = ptcb;     // Link thread to its PTCB
    wakeup(ptcb->thread);         // If everything is done, wakeup the thread
  }
  
	return (Tid_t) ptcb->thread; // NOT current thread.
}

/**
  @brief Return the Tid of the current thread.
 */
Tid_t sys_ThreadSelf()
{
	return (Tid_t) CURTHREAD;
}

/**
  @brief Join the given thread.
  */
int sys_ThreadJoin(Tid_t tid, int* exitval)
{
	return -1;
}

/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{
	return -1;
}

/**
  @brief Terminate the current thread.
  */
void sys_ThreadExit(int exitval)
{

}

/**
  @brief This function is provided as an argument 
  to spawn, to execute the thread of a process
  */
void start_thread_func()
{

  Task call = CURTHREAD->owner_ptcb->main_task;
  int argl = CURTHREAD->owner_ptcb->argl;
  void* args = CURTHREAD->owner_ptcb->args;

  //@TODO need to save exitval in PTCB, probably inside ThreadExit()
  int exitval = call(argl,args);
  CURTHREAD->owner_ptcb->exitval = exitval;
  //@TODO do we do this
  sys_ThreadExit(exitval);
}