#include "types.h"
#include "user.h"
#include "xthread.h"


void xthread_create(int * tid, void * (* start_routine)(void *), void * arg)
{
	
    int id=10;
	char* p_stack=(char* )malloc(4096);
	p_stack+=4096;
	id=clone(start_routine,p_stack,arg);
	*tid=id;
	printf(1,"%tid=%d\n",id);
}


void xthread_exit(void * ret_val_p)
{
    // add your implementation here ...
    
}


void xthread_join(int tid, void ** retval)
{
    // add your implementation here ...
    
}
