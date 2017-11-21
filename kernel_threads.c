
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "kernel_cc.h"

void start_thread_func();

  /**
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void* args)
{
  PTCB* ptcb = Create_PTCB(CURPROC);

  /* Set the thread's function */
  ptcb->main_task = task;

  ptcb->argl = argl;
  ptcb->args = args;

  // Spawn thread
  if(task != NULL)
  { 
    ptcb->thread = spawn_thread(CURPROC, start_thread_func);
    ptcb->thread->owner_ptcb = ptcb;     // Link thread to its PTCB
    
    Mutex_Lock(&CURPROC->thread_mx);
    rlist_push_back(& CURPROC->ptcb_list, & ptcb->pthread);     // Add PThread to parent PCB's list
    ptcb->owner_pcb->thread_count++;
    Mutex_Unlock(&CURPROC->thread_mx);

    wakeup(ptcb->thread);         // If everything is done, wakeup the thread
  }
  else
  {
    free(ptcb);
    return NOTHREAD;
  }

	return (Tid_t) ptcb->thread;    // NOT current thread.
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
  // If thread doesn't exist.
  if(tid == NOTHREAD)
    return -1;

  // local copy for speed reasons
  PCB* process = CURPROC;
  PTCB* ptcb = ((TCB*)tid)->owner_ptcb;
  
  // thread not belonging to proccess
  if (((TCB*)tid)->owner_pcb != process)
  // if (rlist_find(& process->ptcb_list, (void*)tid, (rlnode*)0))
    return -1;
  
  // can't join current or main threads
  if ((TCB*)tid == CURTHREAD || (TCB*)tid == process->main_thread->thread)
    return -1;

  // can't join detached thread but the main thread to be able too
  if (ptcb->detached == 1 && (TCB*)tid != process->main_thread->thread)
    return -1;
  
  ptcb->waiting_threads++;
  
  // Wake up the thread so that he can join us
  Cond_Broadcast(& ptcb->waiting);
  kernel_wait(&ptcb->thread_join, SCHED_USER);
  
  // Make sure exitval is saved.
  if(exitval != NULL)
    *exitval = ptcb->exitval; 

  // Updated counter.
  ptcb->waiting_threads--; 

  if (ptcb->waiting_threads == 0)
  {
    Mutex_Lock(&CURPROC->thread_mx);
    
    process->thread_count--;
    rlist_remove(& ptcb->pthread);
    free(ptcb);

    Mutex_Unlock(&CURPROC->thread_mx);
  }

  return 0;
}

/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{
  // thread doesn't exist
  if( tid == NOTHREAD)
    return -1;

  PTCB* ptcb = ((TCB*)tid)->owner_ptcb;

  // thread is EXITED
  if( ((TCB*)tid)->state == EXITED)
    return -1;
  
  ptcb->detached = 1;

  // Check for joined threads and wake them up
  if (ptcb->waiting_threads > 0)
  { 
    Cond_Broadcast(& ptcb->thread_join);    // Wake up threads.
  }

	return 0;
}

/**
  @brief Terminate the current thread. 

  If it's the main thread, first wait all other threads to finish their Task. 
  Then free its PTCB and finally Exit() the process.

  Otherwise, save exitval in PTCB, broadcast all threads waiting on this thread,
  free PTCB after, set thread status as EXITED and release the kernel.
  */
void sys_ThreadExit(int exitval)
{
  // local copy for speed reasons
  PCB* pcb = CURPROC;
  PTCB* ptcb = CURPTHREAD;

  /* --- If main_thread --- */
  if(CURTHREAD == pcb->main_thread->thread)
  { 
    // Wait for all threads to finish their task.
    while(!is_rlist_empty(&pcb->ptcb_list))
    { 
      // Pick first thread from list.
      PTCB* ptcb_i = pcb->ptcb_list.next->ptcb;
      TCB* tcb_i = ptcb_i->thread;
      
      // Wait thread_i to finish.
      sys_ThreadJoin( (Tid_t) tcb_i, NULL);
    }
    /* All threads should be exited by now */
  }
  else /* --- If NOT main_thread --- */
  { 
    //@TODO what if the thread was detached?
    /* Save exitval in PTCB. */
    ptcb->exitval = exitval;

    // If noone is waiting or us lets wait for them
    if (ptcb->waiting_threads == 0)
    {
      kernel_wait(&ptcb->waiting, SCHED_USER);
    }

    /* sets thread as detached while also waking up any joined threads.*/
    sys_ThreadDetach((Tid_t) CURTHREAD);

    // goodbye cruel world
    kernel_sleep(EXITED, SCHED_USER);
  }
}

/**
  @brief This function is provided as an argument 
  to spawn, to execute the thread of a process
  */
void start_thread_func()
{
  Task call = CURPTHREAD->main_task;
  int argl = CURPTHREAD->argl;
  void* args = CURPTHREAD->args;

  int exitval = call(argl,args);
  ThreadExit(exitval);
}


PTCB* Create_PTCB(PCB* pcb)
{

  PTCB* ptcb = (PTCB*)malloc(sizeof(PTCB));               // Allocate memory
  CHECK((ptcb==NULL)?-1:0);
  memset(ptcb, 0, sizeof(PTCB));

  ptcb->owner_pcb = pcb;
  ptcb->waiting = COND_INIT;                              // Init CondVar
  ptcb->thread_join = COND_INIT;                          

  rlnode_init(& ptcb->pthread, ptcb);                     // Init rlNode

  return ptcb;
}