
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"

void start_thread_func();

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

  ptcb->pthread_mx = MUTEX_INIT;             
  ptcb->thread_join = COND_INIT;                          // Init CondVar
  ptcb->main_task = task;
  ptcb->argl = argl;
  ptcb->args = args;

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
  // local copy for speed reasons
  PCB* process = CURPROC;
  PTCB* ptcb = ((TCB*)tid)->owner_ptcb;

  // thread not belonging to proccess
	if (rlist_find(& process->ptcb_list, (void*)tid, (rlnode*)0))
    return -1;
  
  // can't join current thread
  if ((TCB*)tid == CURTHREAD)
    return -1;

  // can't join detached
  if (ptcb->detached)
    return -1;

  Mutex_Lock(&ptcb->pthread_mx);
  ptcb->waiting_threads++;

  Cond_Wait(&ptcb->pthread_mx, &ptcb->thread_join);

  ptcb->waiting_threads--;
  Mutex_Unlock(&ptcb->pthread_mx);

  *exitval = ptcb->exitval;

  return 0;
}

/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{
	return 0;
}

/**
  @brief Terminate the current thread.
  */
void sys_ThreadExit(int exitval)
{
  // local copy for speed reasons
  PTCB* ptcb = CURTHREAD->owner_ptcb;
  ptcb->exitval = exitval;


  if (!ptcb->detached)
  {
    // don't let anyone join after this
    ptcb->detached = 1;

    // wake everyone so that they can get the retval
    Cond_Broadcast(&ptcb->thread_join);

    // wait for everone to finish joining
    while(ptcb->waiting_threads)
    {
      // hand off to scheduller so that everone can be woken up
      yield(SCHED_USER);
    }
  }

  // do all the deallocation before the thread dies
  free_p_thread(ptcb, sizeof(PTCB));

  // pass to scheduller so that he can kill us
  CURTHREAD->state = EXITED;
  yield(SCHED_USER);
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
  sys_ThreadExit(exitval);
}