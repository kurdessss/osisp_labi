#define _POSIX_C_SOURCE 199506L
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/random.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_RING_SIZE 256

typedef struct _message {
    uint8_t type;
    uint16_t hash;
    uint8_t size;
    char data[259];
} Message;

typedef struct _Ring {
    Message messages[MAX_RING_SIZE];
    int head,
        tail,
        count,
        add,
        get;
} Ring;

pthread_mutex_t mutexp = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexc = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t condp = PTHREAD_COND_INITIALIZER;
pthread_cond_t condc = PTHREAD_COND_INITIALIZER;

uint16_t hash_16(const void* data, size_t len) {
    const uint8_t* bytes = data;
    uint16_t hash = 0xFFFF;
    const uint16_t fnv_prime = 0x0101;

    for (size_t i = 0; i < len; ++i) {
        hash = hash * fnv_prime;
        hash = hash ^ bytes[i];
    }

    return hash;
}

Message generateMessage() {
    srand((unsigned)time(NULL));
    const char letters[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

    Message message;
    {
        uint16_t random;
        do {
            if (getrandom(&random, sizeof(random), 0) < 0) {
                perror("getrandom");
                exit(1);
            }
            message.size = random = random % 257;
            if (random != 0)
                break;

        } while (1);

        for (int i = 0; i < random; message.data[i] = letters[rand() % 53], i++)
            ;
    }

    message.hash = hash_16(message.data, message.size);
    message.type = rand() % 256;

    return message;
}

Ring ring;
int num_prod_threads = 0,
    num_cons_threads = 0;
int ring_size = 10;

void mutex_unlock(void* lock) {
    pthread_mutex_unlock(lock);
}

void* producer(void* arg) {
    pthread_cleanup_push(mutex_unlock, &mutexp);
    int thread_id = *(int*)arg;

    while (1) {
        pthread_mutex_lock(&mutexp);
        while (ring.count == ring_size)
            pthread_cond_wait(&condp, &mutexp);

        if (ring.count < ring_size) {
            ring.messages[ring.head] = generateMessage();

            printf("Producer %d: Create message %s\n", thread_id, ring.messages[ring.head].data);

            ring.head = (ring.head + 1) % ring_size;
            ring.count++;
            ring.add++;
        }
        pthread_cond_signal(&condc);
        pthread_mutex_unlock(&mutexp);
        sleep(3);
    }

    pthread_cleanup_pop(0);
    return NULL;
}

void* consumer(void* arg) {
    pthread_cleanup_push(mutex_unlock, &mutexc);

    int thread_id = *(int*)arg;
    while (1) {
        pthread_mutex_lock(&mutexc);
        while (ring.count == 0)
            pthread_cond_wait(&condc, &mutexc);

        Message message = ring.messages[ring.tail];

        ring.tail = (ring.tail + 1) % ring_size;
        ring.count--;
        ring.get++;

        printf("Consumer %d: Extracted message %s\n", thread_id, message.data);
        pthread_cond_signal(&condp);

        pthread_mutex_unlock(&mutexc);
        sleep(3);
    }

    pthread_cleanup_pop(0);
    return NULL;
}

int main() {
    const int max_prod = 10,
              max_cons = 10;
    pthread_t cons_threads[max_cons];
    pthread_t prod_threads[max_prod];

    ring.head = 0;
    ring.tail = 0;
    ring.count = 0;
    ring.add = 0;
    ring.get = 0;

    char input[3] = {'\0'};
    bool wh = true;

    puts("P - create producer thread\nC - create consumer thread\n-(P/C) - end last created producer/consumer thread\n+/- - increase/decrease ring size\nS - show stat\nQ - end thread and exit\n");

    while (wh) {
        strcpy(input, "\0\0\0");
        scanf("%s", input);
        switch (input[0]) {
            case '+':
                if (ring_size < MAX_RING_SIZE) {
                    ring_size++;
                    printf("Ring size increased to %d\n", ring_size);
                } else {
                    printf("Ring size cannot be increased\n");
                }
                break;

            case '-':
                if (input[1] == 'p' || input[1] == 'P') {
                    if (num_prod_threads > 0) {
                        pthread_cancel(prod_threads[num_prod_threads - 1]);
                        pthread_join(prod_threads[num_prod_threads-- - 1], NULL);
                    } else
                        puts("Count of producers equals 0");
                    break;
                } else if (input[1] == 'c' || input[1] == 'C') {
                    if (num_cons_threads > 0) {
                        pthread_cancel(cons_threads[num_cons_threads - 1]);
                        pthread_join(cons_threads[num_cons_threads-- - 1], NULL);
                    } else
                        puts("Count of consumers equals 0");
                    break;
                }

                if (ring_size > 1) {
                    ring_size--;
                    printf("Ring size decreased to %d\n", ring_size);
                } else {
                    printf("Ring size cannot be decreased\n");
                }
                break;
            case 'P':
            case 'p':
                if (num_prod_threads < max_prod) {
                    if (pthread_create(&prod_threads[++num_prod_threads - 1], NULL, producer, &num_prod_threads) != 0)
                        perror("Producer_pthread_create error");
                }
                break;
            case 'C':
            case 'c':
                if (num_cons_threads < max_cons) {
                    if (pthread_create(&cons_threads[++num_cons_threads - 1], NULL, consumer, &num_cons_threads) != 0)
                        perror("Consumer_pthread_create error");
                }
                break;
            case 'S':
            case 's':
                printf("---Stat---\nRing size: %d\nCount: %d\nAdded: %d\nGetted: %d\nProducers count: %d\nConsumers count: %d\n", ring_size, ring.count, ring.add, ring.get, num_prod_threads, num_cons_threads);
                break;
            case 'Q':
            case 'q':
                while (num_prod_threads) {
                    pthread_cancel(prod_threads[num_prod_threads - 1]);
                    pthread_join(prod_threads[num_prod_threads-- - 1], NULL);
                };
                while (num_cons_threads) {
                    pthread_cancel(cons_threads[num_cons_threads - 1]);
                    pthread_join(cons_threads[num_cons_threads-- - 1], NULL);
                }
                wh = false;
                break;
        }
    }

    return 0;
}
