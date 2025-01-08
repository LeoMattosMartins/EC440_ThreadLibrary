#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <assert.h>

#define NUM_THREADS 3

sem_t testSemaphore;

void *thread_function(void *arg);
void *producer_function(void *arg);
void *consumer_function(void *arg);

int main()
{
    pthread_t threads[NUM_THREADS];
    int result, i;
    void *ret_val;

    for(i = 0; i < NUM_THREADS; i++)
    {
        result = pthread_create(&threads[i], NULL, thread_function, (void *)(long)i);
        assert(result == 0); /* Assert that thread creation succeeded */
    }

    /* Join Threads */
    for(i = 0; i < NUM_THREADS; i++)
    {
        result = pthread_join(threads[i], &ret_val);
    }
    result = pthread_create(&threads[0], NULL, thread_function, (void *)100);

    result = pthread_create(&threads[1], NULL, thread_function, (void *)200);

    result = pthread_join(threads[0], NULL);

    result = pthread_join(threads[1], NULL);

    result = sem_init(&testSemaphore, 0, 1);

    result = sem_wait(&testSemaphore); /* Should decrement count */

    result = sem_post(&testSemaphore); /* Should increment count */

    result = sem_destroy(&testSemaphore);

    result = sem_init(&testSemaphore, 0, 0); /* Semaphore initialized with 0 */

    pthread_t producer, consumer;
    result = pthread_create(&producer, NULL, producer_function, NULL);

    result = pthread_create(&consumer, NULL, consumer_function, NULL);

    pthread_join(producer, NULL);
    pthread_join(consumer, NULL);

    result = sem_destroy(&testSemaphore);

    return 0;
}

void *thread_function(void *arg)
{
    int num = (int)(long)arg;
    int result = num * num;
    printf("Thread %d: returning %d\n", num, result);
    pthread_exit((void *)(long)result);
}

void *producer_function(void *arg)
{
    (void)arg;
    printf("Producer: Producing an item.\n");
    sem_post(&testSemaphore);
    pthread_exit(NULL);
}

void *consumer_function(void *arg)
{
    (void)arg;
    printf("Consumer: Waiting for an item.\n");
    sem_wait(&testSemaphore);
    printf("Consumer: Consumed an item.\n");
    pthread_exit(NULL);
}
