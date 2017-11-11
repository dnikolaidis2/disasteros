
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

  ptcb->main_task = task;
  ptcb->argl = argl;
  ptcb->args = args;

  // Spawn thread
  if(task != NULL)
  {
    ptcb->thread = spawn_thread(CURPROC, start_thread_func);
    ptcb->thread->owner_ptcb = ptcb;     // Link thread to its PTCB
    
    ptcb->thread->owner_pcb->thread_count++;

    if(!wakeup(ptcb->thread))         // If everything is done, wakeup the thread
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
  int detached_flag;

  // local copy for speed reasons
  PCB* process = CURPROC;
  PTCB* ptcb = ((TCB*)tid)->owner_ptcb;

  // thread not belonging to proccess
	if (rlist_find(& process->ptcb_list, (void*)tid, (rlnode*)0))
    return -1;
  
  // can't join current or main threads
  if ((TCB*)tid == CURTHREAD || (TCB*)tid == process->main_thread)
    return -1;

  // can't join detached 
  if (ptcb->detached == 1)
    return -1;

  Mutex_Lock(&ptcb->pthread_mx);
  ptcb->waiting_threads++;

  Cond_Wait(&ptcb->pthread_mx, &ptcb->thread_join);
  
  // Make sure exitval is saved.
  if(exitval != NULL)
    *exitval = ptcb->exitval; 
  
  // Check if woke up because of detach
  // Need to be checked before "waiting_t--" to prevent race cond.
  if(ptcb->detached)
    detached_flag = 1;

  // Updated counter.
  ptcb->waiting_threads--; 
  
  Mutex_Unlock(&ptcb->pthread_mx);

  
  if (detached_flag){
    return -1;
  }
  return 0;
}

/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{

  PTCB* ptcb = ((TCB*)tid)->owner_ptcb;

  // thread doesn't exist
  if( tid == NOTHREAD)
  {
    return -1;
  }

  // thread is EXITED
  if( ((TCB*)tid)->state == EXITED)
  {
    return -1;
  }

  // Check for joined threads and wake them up
  if (ptcb->waiting_threads > 0)
  { 
    //@TODO maybe we need a lock?
    Cond_Broadcast(& ptcb->thread_join);    // Wake up threads.
    
    // hand off to scheduller so that everone can be woken up
    while(ptcb->waiting_threads > 0)
    {      
      yield(SCHED_USER);
    }
  }

  ptcb->detached = 1;

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
  PCB* pcb = CURTHREAD->owner_pcb;
  PTCB* ptcb = CURTHREAD->owner_ptcb;

  /* --- If main_thread --- */
  if(CURTHREAD == pcb->main_thread)
  { 
    // Wait for all threads to finish their task.
    for(int i=0; i< pcb->thread_count; i++)
    { 
      // Pick first thread from list.
      PTCB* ptcb_i = (PTCB*) rlist_pop_front(& pcb->ptcb_list);
      TCB* tcb_i = ptcb_i->thread;
      
      // Skip if CURTHREAD.
      if(tcb_i == CURTHREAD)
        continue;

      // Wait thread_i to finish.
      sys_ThreadJoin( (Tid_t) tcb_i, NULL);

    }

    /* All other threads should be exited by now */

    /* We no longer need the PTCB */
    free(ptcb);

    /* thread_count should now be =0*/
    pcb->thread_count--;
    /* Exit the process */
    Exit(exitval);

  }
  else /* --- If NOT main_thread --- */
  { 
    /* Save exitval in PTCB. */
    ptcb->exitval = exitval;

    /* sets thread as detached while also waking up any joined threads.*/
    sys_ThreadDetach((Tid_t) CURTHREAD);

    /* We no longer need the PTCB */
    free(ptcb);

    pcb->thread_count--;
    kernel_sleep(EXITED, SCHED_USER);
  }

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

  int exitval = call(argl,args);
  sys_ThreadExit(exitval);
}


PTCB* Create_PTCB(PCB* pcb)
{

  PTCB* ptcb = (PTCB*)malloc(sizeof(PTCB));               // Allocate memory
  CHECK((ptcb==NULL)?-1:0);
  memset(ptcb, 0, sizeof(PTCB));

  ptcb->pthread_mx = MUTEX_INIT;             
  ptcb->thread_join = COND_INIT;                          // Init CondVar

  rlnode_init(& ptcb->pthread, ptcb);                     // Init rlNode
  rlist_push_back(& pcb->ptcb_list, & ptcb->pthread);     // Add PThread to parent PCB's list

  return ptcb;
}