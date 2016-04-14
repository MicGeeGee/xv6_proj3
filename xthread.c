#include "types.h"
#include "user.h"
#include "xthread.h"


void xthread_create(int * tid, void * (* start_routine)(void *), void * arg)
{
	int id;
	char* p_stack=(char* )malloc(4096);
	p_stack+=4096;
	id=clone(start_routine,p_stack,arg);
	*tid=id;
	
}

void xthread_join(int tid, void ** retval)
{
	void** stack=0;
    join(tid,retval,stack);
	free(*stack);
}

void xthread_exit(void * ret_val_p)
{
	thread_exit(ret_val_p);    
}



