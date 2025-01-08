#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <semaphore.h>
#include "ec440threads.h"

#define JB_R12 2
#define JB_R13 3
#define JB_RSP 6
#define JB_PC 7

#define MAX_THREADS 128
#define STACK_SIZE 32767
#define QUANTUM 50000

#define READY 0
#define RUNNING 1
#define EXITED 2
#define BLOCKED 3

#define ALARM 0
#define TIMER 1
#define CREATE_ARGS 2
#define CREATE_MAX 3
#define CREATE_STACK 4
#define CREATE_SETJMP 5
#define SCHEDULE_SETJMP 6
#define SCHEDULE_LONGJMP 7
#define EXIT_SCHEDULE 8
#define SEM_INIT 9
#define SEM_DESTROY 10
#define SEM_WAIT_NOT_INIT 11
#define SEM_POST_NOT_INIT 12

typedef struct
{
        jmp_buf context;
        void *stack;
        void *arg;
        void *(*start_routine)(void *);
        int status;
} TCB;

typedef struct Node
{
        int threadId;
        struct Node *next;
} Node;

typedef struct
{
        int count;
        int initialized;
        Node *head;
        Node *tail;

} Semaphore;

TCB tcb[MAX_THREADS];
int currentThread = 0;
int threadCount = 1;

void enqueue(Node **head, Node **tail, int threadId)
{
        Node *newNode = (Node *)malloc(sizeof(Node));
        (*newNode).threadId = threadId;
        (*newNode).next = NULL;

        if( *tail ) { (**tail).next = newNode; }
        else { *head = newNode; }

        *tail = newNode;
}

int dequeue(Node **head, Node **tail)
{
        if( !*head ) { return -1; }

        Node *temp = *head;
        int threadId = (*temp).threadId;
        *head = (**head).next;

        if( !*head ) {  *tail = NULL; }

        free(temp);
        return threadId;
}

void ohNo(int error)
{
        switch( error )
        {
                case ALARM:             perror("ERROR: sigaction() failed during alarm setup\n"); break;
                case TIMER:             perror("ERROR: setitimer() failed during timer setup\n"); break;
                case CREATE_ARGS:       perror("ERROR: invalid arguments provided in pthread_create\n"); break;
                case CREATE_MAX:        perror("ERROR: maximum thread count exceeded in pthread_create\n"); break;
                case CREATE_STACK:      perror("ERROR: malloc failed to allocate stack for new thread\n"); break;
                case CREATE_SETJMP:     perror("ERROR: setjmp failed during thread creation in pthread_create\n"); break;
                case SCHEDULE_SETJMP:   perror("ERROR: setjmp failed in schedule function\n"); break;
                case SCHEDULE_LONGJMP:  perror("ERROR: longjmp failed in schedule function\n"); break;
                case EXIT_SCHEDULE:     perror("ERROR: schedule call failed in pthread_exit\n"); break;
                case SEM_INIT:          perror("ERROR: semaphore initialization failed in sem_init\n"); break;
                case SEM_DESTROY:       perror("ERROR: attempting to destroy an uninitialized semaphore in sem_destroy\n"); break;
                case SEM_WAIT_NOT_INIT: perror("ERROR: attempted wait on uninitialized semaphore in sem_wait\n"); break;
                case SEM_POST_NOT_INIT: perror("ERROR: attempted post on uninitialized semaphore in sem_post\n"); break;
                default:                perror("ERROR: unknown error occurred\n"); break;
        }
        exit(2);
}

void lock()
{
        sigset_t alarmMask;
        sigemptyset(&alarmMask);
        sigaddset(&alarmMask, SIGALRM);

        if( sigprocmask(SIG_BLOCK, &alarmMask, NULL) == -1 ) { ohNo(ALARM); }
}

void unlock()
{
        sigset_t alarmMask;
        sigemptyset(&alarmMask);
        sigaddset(&alarmMask, SIGALRM);

        if( sigprocmask(SIG_UNBLOCK, &alarmMask, NULL) == -1 ) { ohNo(ALARM); }
}

pthread_t pthread_self(void) { return currentThread; }

void schedule(int a)
{
        (void) a;
        int nextThread, ii, blocking = 0;

        nextThread = (currentThread + 1) % threadCount;

        if( tcb[currentThread].status != EXITED ) { tcb[currentThread].status = READY; }

        for( ii = 0; ii <= threadCount; ii++ ) { if( tcb[ii].status == BLOCKED ) { blocking = 1; break; } }

        while( tcb[nextThread].status != READY )
        {
                if( (tcb[nextThread].status == EXITED) && (blocking) ) { free(tcb[nextThread].stack); }
                nextThread = (nextThread + 1) % threadCount;
        }

        if( !setjmp(tcb[currentThread].context) )
        {
                currentThread = nextThread;
                tcb[nextThread].status = RUNNING;
                longjmp(tcb[nextThread].context, 1);
                ohNo(SCHEDULE_LONGJMP);
        }
}

void setupTimer()
{
        struct sigaction sa;
        struct itimerval timer;

        sa.sa_handler = &schedule;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_NODEFER;

        if( sigaction(SIGALRM, &sa, NULL) == -1 ) { ohNo(ALARM); }

        timer.it_value.tv_sec = 0;
        timer.it_value.tv_usec = QUANTUM;
        timer.it_interval.tv_sec = 0;
        timer.it_interval.tv_usec = QUANTUM;

        if( setitimer(ITIMER_REAL, &timer, NULL) == -1 ) { ohNo(TIMER); }
}

void pthread_exit_wrapper()
{
        unsigned long int res;
        asm("movq %%rax, %0\n":"=r"(res));
        pthread_exit((void*) res);
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg)
{
        int returnValue, id;
        unsigned long int *stackTop;

        (void)attr;
        returnValue = 0;

        if( !thread || !start_routine ) { ohNo(CREATE_ARGS); }
        if( threadCount >= MAX_THREADS ) { ohNo(CREATE_MAX); }

        id = threadCount++;
        tcb[id].stack = malloc(STACK_SIZE);
        tcb[id].status = READY;
        tcb[id].arg = arg;
        tcb[id].start_routine = start_routine;

        if( !tcb[id].stack ) { ohNo(CREATE_STACK); }

        if( !setjmp(tcb[id].context) )
        {
                stackTop = (unsigned long int *)(tcb[id].stack + STACK_SIZE - sizeof(unsigned long int));
                *stackTop = (unsigned long int) pthread_exit_wrapper;

                tcb[id].context[0].__jmpbuf[JB_RSP] = ptr_mangle((unsigned long int)stackTop);
                tcb[id].context[0].__jmpbuf[JB_PC]  = ptr_mangle((unsigned long int)start_thunk);
                tcb[id].context[0].__jmpbuf[JB_R13] = ((unsigned long int)arg);
                tcb[id].context[0].__jmpbuf[JB_R12] = ((unsigned long int)start_routine);

                *thread = id;
        }
        else
        {
                free(tcb[id].stack);
                returnValue = -1;
        }

        return returnValue;
}

void pthread_exit(void *valuePtr)
{
        int ii, theEnd;

        tcb[currentThread].status = EXITED;
        tcb[currentThread].arg = valuePtr;
        theEnd = 1;

        for( ii = 1; ii <= threadCount; ii++ )
        {
                if( tcb[ii].status == BLOCKED && ii != currentThread ) { tcb[ii].status = READY; }
        }

        for( ii = 1; ii <= threadCount; ii++ )
        {
                if( tcb[ii].status != EXITED )
                {
                        theEnd = 0;
                        break;
                }
        }

        if( theEnd ) { exit(0); }

        schedule(0);
        __builtin_unreachable();
}

int pthread_join(pthread_t thread, void **valuePtr)
{
        int returnValue;

        returnValue = 0;
        thread = (unsigned long int) thread;

        if( thread >= threadCount || tcb[thread].status == EXITED ) return -1;

        while( tcb[thread].status != EXITED )
        {
                tcb[currentThread].status = BLOCKED;
                /* schedule(0); */
        }

        if( valuePtr != NULL ) { *valuePtr = tcb[thread].arg; }

        free(tcb[thread].stack);

        return returnValue;
}

int sem_init(sem_t *sem, int pshared, unsigned value)
{
        Semaphore *sema;

        (void) pshared;

        sema = (Semaphore *)malloc(sizeof(Semaphore));

        if( !sem ) { ohNo(SEM_INIT); }

        (*sema).count = value;
        (*sema).initialized = 1;
        (*sema).head = NULL;
        (*sema).tail = NULL;

        *(void **)sem = sema;

        return 0;
}

int sem_wait(sem_t *sem)
{
        Semaphore *sema = *(Semaphore **)sem;

        if( !sema || !(*sema).initialized ) { ohNo(SEM_WAIT_NOT_INIT); }

        lock();

        if( (*sema).count <= 0 )
        {
                enqueue(&sema->head, &sema->tail, currentThread);
                tcb[currentThread].status = BLOCKED;
                unlock();
                schedule(0);
                lock();
        }

        (*sema).count = (*sema).count - 1;

        unlock();

        return 0;
}

int sem_post(sem_t *sem)
{
        Semaphore *sema = *(Semaphore **)sem;
        int nextThread;

        if( !sema || !(*sema).initialized ) { ohNo(SEM_POST_NOT_INIT); }

        lock();

        (*sema).count = (*sema).count + 1;

        if( ((*sema).count > 0)  && ((*sema).head) )
        {
                nextThread = dequeue(&sema->head, &sema->tail);

                if( nextThread >= 0 ) { tcb[nextThread].status = READY; }
        }

        unlock();

        return 0;
}

int sem_destroy(sem_t *sem)
{
        Semaphore *sema = *(Semaphore **)sem;

        if( !sema || !(*sema).initialized ) { ohNo(SEM_DESTROY); }

        (*sema).initialized = 0;
        *(void **)sema = NULL;

        return 0;
}

__attribute__((constructor)) void initThreads()
{
        setupTimer();
        tcb[0].status = RUNNING;
}                                     
