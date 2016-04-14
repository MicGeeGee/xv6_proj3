#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return proc->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = proc->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;
  
  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(proc->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;
  
  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}


int sys_clone(void)
{
	void* (*fn)(void *)=0;
	void* stack=0;
	void* arg=0;
	
	int n1;
	int n2;
	int n3;
	
	argint(0,&n1);
	argint(1,&n2);
	argint(2,&n3);

	
	
	fn=(void* (*)(void *))n1;
	stack=(void* )n2;
	arg=(void* )n3;
	
	return clone(fn,stack,arg);
}
int sys_join(void)
{
	int tid;
	void** ret_p;
	void** stack;
	
	int n1;
	int n2;
	int n3;
	
	argint(0,&n1);
	argint(1,&n2);
	argint(2,&n3);

	tid=n1;
	ret_p=(void** )n2;
	stack=(void** )n3;


	join(tid,ret_p,stack);

	return 0;
}
int sys_thread_exit(void)
{
	void* ret;
	int n;
	argint(0,&n);
	ret=(void* )n;

	thread_exit(ret);

	return 0;
}