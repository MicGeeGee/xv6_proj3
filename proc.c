#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;

extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;
  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;
  
  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;
  
  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];
  
  p = allocproc();
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  
  sz = proc->sz;
  if(n > 0){
    if((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  proc->sz = sz;
  switchuvm(proc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;

  // Allocate process.
  if((np = allocproc()) == 0)
    return -1;

  // Copy process state from p.
  if((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);

  safestrcpy(np->name, proc->name, sizeof(proc->name));
 
  pid = np->pid;

  // lock to force the compiler to emit the np->state write last.
  acquire(&ptable.lock);
  np->state = RUNNABLE;
  release(&ptable.lock);
  
  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *p;
  int fd;
  int is_thread_running;

  if(proc == initproc)
    panic("init exiting");

  is_thread_running=0;
  if(!proc->xstack)
  {
	  // proc is not a thread but a process.
	  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
	  {
		if(p == proc)
			continue;
		if(p->parent == proc && p->xstack)
		{
			// Get a child thread.
			if(p->state != ZOMBIE)
			{
				is_thread_running=1;
				break;
			}
		}
	  }
  }
  else
  {
	  // proc is a thread.
	  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
	  {
		if(p == proc)
			continue;
		if(p->parent == proc->parent && p->xstack)
		{
			// Get a brother thread.
			if(p->state != ZOMBIE)
			{
				is_thread_running=1;
				break;
			}
		}
	  }

	  if(proc->parent->state != ZOMBIE)
	    is_thread_running=1;
  }

  if(!is_thread_running)
  {
	  // If there is no child thread running,
	  // release the resources.
	  // Close all open files.
	  for(fd = 0; fd < NOFILE; fd++)
	  {
		if(proc->ofile[fd])
		{
		  fileclose(proc->ofile[fd]);
		  proc->ofile[fd] = 0;
		}
	  }

	  begin_op();
	  iput(proc->cwd);
	  end_op();
	  proc->cwd = 0;
  
	  acquire(&ptable.lock);

	  // Parent might be sleeping in wait().
	  // If the the wakeup1 is not been invoked,
	  // the zombie will not be put away.
	  wakeup1(proc->parent);
  }
  else
	  acquire(&ptable.lock);
   
  // Pass abandoned children to init.
  // This code is for proc's children.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
	  if(p->parent == proc && !p->xstack)
	  {
		p->parent = initproc;
		if(p->state == ZOMBIE)
		wakeup1(initproc);
	  }
  }


  // Jump into the scheduler, never to return.
  // This code is for proc's parent, 
  // in other words, to let its parent put away the zombie.
  proc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
	  // Wait is not responsible for threads.
      if(p->parent != proc || p->xstack)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->state = UNUSED;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;

  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      
	  //if(p->xstack)
		//cprintf("thread_exit:pid=%d,esp=0x%x\n",p->pid,p->tf->esp);

	  if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      proc = p;

	  

      switchuvm(p);
      p->state = RUNNING;

	  

      swtch(&cpu->scheduler, proc->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      proc = 0;
    }
    release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state.
void
sched(void)
{
  int intena;

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(cpu->ncli != 1)
    panic("sched locks");
  if(proc->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;

  swtch(&proc->context, cpu->scheduler);
  cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
 
  acquire(&ptable.lock);  //DOC: yieldlock
  proc->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot 
    // be run from main().
    first = 0;
    initlog();
  }
  
  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  if(proc == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }

  // Go to sleep.
  proc->chan = chan;
  proc->state = SLEEPING;
  sched();

  // Tidy up.
  proc->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];
  
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}


int 
clone(void* (*fn)(void *),void* stack, void* arg)
{
	int i;
	uint sp;
	struct proc *np;
	uint ustack[2];

	// Allocate process.
	if((np = allocproc()) == 0)
		return -1;
	

	// Share process state from p.
	np->pgdir=proc->pgdir;

	np->sz = proc->sz;
	np->parent = proc;
	*np->tf = *proc->tf;
	
	// Set start function.
	np->tf->eip=(uint)fn;

	
	np->xstack=(char* )stack;
	// Implement caller's responsibilities.
	sp=(uint)stack;
	

	//sp=sp-4;
	//if(copyout(np->pgdir,sp,&arg,sizeof(char*))<0)
		//return -2;
	//sp=sp-4;
	//ret_addr=0xffffffff;
	//if(copyout(np->pgdir, sp, &ret_addr, sizeof(uint)) < 0)
		//return -3;
	
	ustack[0]=0xffffffff;
	ustack[1]=(uint)arg;
	
	sp-=2*4;
	if(copyout(np->pgdir, sp, ustack, 2*4) < 0)
		return -2;

	np->tf->esp=sp;
	
	
	for(i = 0; i < NOFILE; i++)
		if(proc->ofile[i])
			np->ofile[i] = filedup(proc->ofile[i]);
	np->cwd = idup(proc->cwd);

	safestrcpy(np->name, proc->name, sizeof(proc->name));
	
	// lock to force the compiler to emit the np->state write last.
	acquire(&ptable.lock);
	np->state = RUNNABLE;
	release(&ptable.lock);
  
	return np->pid;

//bad:
	//return -2;
	
}

void join(int tid,void** ret_p,void** stack)
{
	struct proc *p;
	int is_existed;

	acquire(&ptable.lock);

	for(;;)
	{
		// Scan through table looking for zombie thread whose pid is equal to tid.
		is_existed = 0;
		for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
		{
			if(p->pid != tid || !p->xstack)
				continue;
			is_existed = 1;
			if(p->state == ZOMBIE)
			{
				// Found one.
				kfree(p->kstack);
				p->kstack = 0;

				// Pass the bottom address to stack.
				*stack=p->xstack-4096;

				// Pass the return value to ret_p.
				//cprintf("join:pid=%d,esp=0x%x\n",p->pid,p->tf->esp);
				
				//*ret_p=(void* )(*((uint*)(p->tf->esp)));
				*ret_p=p->xret;


				p->state = UNUSED;
				p->pid = 0;
				p->parent = 0;
				p->name[0] = 0;
				p->killed = 0;
				p->xstack = 0;
				release(&ptable.lock);

				// We could only wait for one thread.
				return;
			}
			else
			{
				// We could only wait for one thread.
				break;
			}
		}

		// No point waiting if we don't have the correspond thread.
		if(!is_existed || proc->killed)
		{
			release(&ptable.lock);
			return;
		}

		// Wait for this thread to exit.  (See wakeup1 call in proc_exit.)
		sleep(&p->pid, &ptable.lock);  //DOC: wait-sleep
	}

}

void thread_exit(void* ret)
{
	//uint sp;
	int is_last;
	int fd;
	struct proc* p;

	if(proc == initproc)
		panic("init exiting");

	// Find out whether this thread is the last one.
	is_last=1;
	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
		if(p == proc)
			continue;
		if(p->parent == proc->parent && p->xstack){
			// Get a brother thread.
			if(p->state != ZOMBIE)
			{
				is_last=0;
				break;
			}
		}
	}
	if(proc->parent->state != ZOMBIE)
		is_last=0;

	if(is_last)
	{
		// If there is no child thread running,
		// release the resources.
		// Close all open files.
		for(fd = 0; fd < NOFILE; fd++){
			if(proc->parent->ofile[fd]){
				fileclose(proc->parent->ofile[fd]);
				proc->parent->ofile[fd] = 0;
				}
		}

		begin_op();
		iput(proc->parent->cwd);
		end_op();
		proc->parent->cwd = 0;
  
		// Parent might be sleeping in wait().
		// If the the wakeup1 is not been invoked,
		// the zombie will not be put away.
		wakeup1(proc->parent->parent);
	}


	acquire(&ptable.lock);

	// Store the return value on the user stack.
	// We only need the address here to get access to the correspond place in pgdir.
	/*
	cprintf("thread_exit:pid=%d,esp=0x%x\n",proc->pid,proc->tf->esp);
	
	sp=proc->tf->esp;
	sp-=4;
	if(copyout(proc->pgdir,sp,&ret,sizeof(&ret))<0)
		panic("cannot store return value");
	proc->tf->esp=sp;
	cprintf("thread_exit:proc ptr=0x%x,pid=%d,esp=0x%x\n",proc,proc->pid,proc->tf->esp);
	*/
	
	// Store the return value.
	proc->xret=ret;
	
	
	// There might be joined thread sleeping in join().
	wakeup1(&proc->pid);

	

	
	// Jump into the scheduler, never to return.
	proc->state = ZOMBIE;
	// Take care of the code here,
	// proc->tf will not be popped.
	sched();

	panic("zombie exit");
}


