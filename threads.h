#ifndef THREADS_H
#define THREADS_H

#include <pthread.h>
#include <semaphore.h>

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

void enqueue(Node **head, Node **tail, int threadId);
int dequeue(Node **head, Node **tail);
void ohNo(int error);
void lock(void);
void unlock(void);
pthread_t pthread_self(void);
void schedule(int a);
void setupTime(void);
void pthread_exit_wrapper(void);
int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg);
void pthread_exit(void *value_ptr);
int pthread_join(pthread_t thread, void **valuePtr);
int sem_init(sem_t *sem, int pshared, unsigned value);
int sem_wait(sem_t *sem);
int sem_post(sem_t *sem);
int sem_destroy(sem_t *sem);


#endif
