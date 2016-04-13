#include "types.h"
#include "user.h"
#include "xthread.h"

int count = 0;

void usage(void)
{
    printf(1, "Usage: tlibtest # (1-5)\n");
}

void add_then_exit(int a, int b)
{
    int n = a + b;
    xthread_exit((void *)n);
}



/***************************** TEST CASE 1 *********************************
The expected output would look like (depending how threads are scheduled, the outputs may be mixed up a little bit):

-------------------- TEST CASE 1 --------------------
Printing in CHILD1 thread: completed!
Printing in CHILD2 thread: completed!
Printing in MAIN thread: count=3 
****************************************************************************/
void * tc1_func(void * arg)
{
    count++;
    printf(1, "Printing in %s thread: completed!\n", (char *)arg);
	
	//exit();
    return (void *)0;

}

void tc1(void)
{
    int tid_1 = 0, tid_2 = 0;

    printf(1, "\n-------------------- TEST CASE 1 --------------------\n");
    count = 0;
    xthread_create(&tid_1, tc1_func, (void *)"CHILD1");
    xthread_create(&tid_2, tc1_func, (void *)"CHILD2");
    count++;
    sleep(100); // 1 second
    printf(1, "Printing in MAIN thread: count=%d\n", count);
}



/***************************** TEST CASE 2 *********************************
The expected output would look like (depending how the threads are scheduled, the outputs may be mixed up a little bit):

-------------------- TEST CASE 2 --------------------
Printing in MAIN thread: completed!
$ Printing in CHILD1 thread: count=3
Printing in CHILD2 thread: count=3
****************************************************************************/
void * tc2_func(void * arg)
{
   count++;
   sleep(100); // 1 second
   printf(1, "Printing in %s thread: count=%d\n", (char *)arg, count);
   return (void *)0;
}

void tc2(void)
{
    int tid_1 = 0, tid_2 = 0;

    printf(1, "\n-------------------- TEST CASE 2 --------------------\n");
    count = 0;
    xthread_create(&tid_1, tc2_func, (void *)"CHILD1");
    xthread_create(&tid_2, tc2_func, (void *)"CHILD2");
    count++;
    printf(1, "Printing in MAIN thread: completed!\n");
}



/***************************** TEST CASE 3 *********************************
The expected output will look like:

-------------------- TEST CASE 3 --------------------
Thread 46 returned 0x12345.
Thread 47 returned 33.
* ***************************************************************************/
void * tc3_func1(void * arg)
{
    return (void *)0x12345;
}

void * tc3_func2(void * arg)
{
    add_then_exit(11, 22);
    return (void *)0;
}

void tc3(void)
{
    int tid_1, tid_2;
    void * ret_val;
    
    printf(1, "\n-------------------- TEST CASE 3 --------------------\n");
    xthread_create(&tid_1, tc3_func1, (void *)0);
    xthread_create(&tid_2, tc3_func2, (void *)0);

    sleep(100); // 1 second

    xthread_join(tid_1, &ret_val);
    printf(1, "Thread %d returned 0x%x.\n", tid_1, (uint)ret_val);

    xthread_join(tid_2, &ret_val);
    printf(1, "Thread %d returned %d.\n", tid_2, (uint)ret_val);      
}



/***************************** TEST CASE 4  *********************************
The expected output would will like:

-------------------- TEST CASE 4 --------------------
Thread 53 returned 0x12345.
Thread 54 returned 33.
* ***************************************************************************/
void * tc4_func1(void * arg)
{
    sleep(100); // 1 second
    return (void *)0x12345;
}

void * tc4_func2(void * arg)
{
    sleep(200); // 2 seconds
    add_then_exit(11, 22);
    return (void *)0;
}

void tc4(void)
{
    int tid_1, tid_2;
    void * ret_val;
    
    printf(1, "\n-------------------- TEST CASE 4 --------------------\n");
    xthread_create(&tid_1, tc4_func1, (void *)0);
    xthread_create(&tid_2, tc4_func2, (void *)0);

    xthread_join(tid_1, &ret_val);
    printf(1, "Thread %d returned 0x%x.\n", tid_1, (uint)ret_val);

    xthread_join(tid_2, &ret_val);
    printf(1, "Thread %d returned %d.\n", tid_2, (uint)ret_val);      
}



/***************************** TEST CASE 5  *********************************
The expected output would will like:

-------------------- TEST CASE 5 --------------------
ptr1 - ptr2 = 16
****************************************************************************/
void * tc5_func(void * arg)
{
    sleep(100); // 1 second
    add_then_exit(0, 0);
    return (void *)0;
}

void tc5(void)
{
    int tid;
    void * ret_val;
    void * ptr1 = (void *)0, * ptr2 = (void *)0;


    printf(1, "\n-------------------- TEST CASE 5 --------------------\n");
    ptr1 = malloc(1);
    xthread_create(&tid, tc5_func, (void *)0);
    xthread_join(tid, &ret_val);
    ptr2 = malloc(1);
    printf(1, "ptr1 - ptr2 = %d\n", ptr1 - ptr2);      

    if (ptr1)
        free(ptr1);
    if (ptr2)
        free(ptr2);    
}


/************************** main() *****************************/
int main(int argc, char *argv[])
{ 
    if (argc < 2)
    {
        usage();
        exit();
    }

    switch (argv[1][0])
    {
        case '1':
            tc1();
            break;

        case '2':
            tc2();
            break;

        case '3':
            tc3();
            break;

        case '4':
            tc4();
            break;

        case '5':
            tc5();
            break;

        default:
            printf(1, "No such test case (%c).\n", argv[1][0]);
            usage();
            break;
    }

    exit();
}


