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
#define max 10
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

        for (int i = 0; i < random; message.data[i] = letters[rand() % 53], i++);
    }

    message.hash = hash_16(message.data, message.size);
    message.type = rand() % 256;

    return message;
}

Ring ring;
sem_t fill_sem, empty_sem;
pthread_mutex_t ring_mutex;
int num_prod_threads = 0,
    num_cons_threads = 0;
//const int max_prod = 10,
  //        max_cons = 10;
pthread_t consumer_threads[max];
pthread_t producer_threads[max];
int ring_size = 10;
char input[3] = {'\0'};
bool wh = true;

void* producer(void* arg) {
    int thread_id = *(int*)arg;
    while (1) {
        sem_wait(&fill_sem);
        pthread_mutex_lock(&ring_mutex);

        if (ring.count != ring_size) {
            ring.messages[ring.head] = generateMessage();

            printf("Producer %d: Create message %s\n", thread_id, ring.messages[ring.head].data);

            ring.head = (ring.head + 1) % ring_size;
            ring.count++;
            ring.add++;
        }

        pthread_mutex_unlock(&ring_mutex);
        sem_post(&empty_sem);
        sleep(3);
    }
    return NULL;
}

void* consumer(void* arg) {
    int thread_id = *(int*)arg;
    while (1) {
        sem_wait(&empty_sem);
        pthread_mutex_lock(&ring_mutex);

        if (ring.count > 0) {
            Message message = ring.messages[ring.tail];

            ring.tail = (ring.tail + 1) % ring_size;
            ring.count--;
            ring.get++;

            printf("Consumer %d: Extracted message %s\n", thread_id, message.data);
        }

        pthread_mutex_unlock(&ring_mutex);
        sem_post(&fill_sem);
        sleep(3);
    }
    return NULL;
}

void increase_size() {
    if (ring_size < MAX_RING_SIZE) {
        ring_size++;
        sem_post(&fill_sem);
        printf("Ring size increased to %d\n", ring_size);
    } else {
        printf("Ring size cannot be increased\n");
    }
} 

void decrease_size() {
    if (input[1] == 'p' || input[1] == 'P') {
        if (num_prod_threads > 0) {
            pthread_cancel(producer_threads[num_prod_threads - 1]);
            pthread_join(producer_threads[num_prod_threads-- - 1], NULL);
        } else
            puts("Count of producers equals 0");
        return;
    } else if (input[1] == 'c' || input[1] == 'C') {
        if (num_cons_threads > 0) {
            pthread_cancel(consumer_threads[num_cons_threads - 1]);
            pthread_join(consumer_threads[num_cons_threads-- - 1], NULL);
        } else
            puts("Count of consumers equals 0");
        return;
    }

    if (ring_size > 1) {
        ring_size--;
        sem_wait(&fill_sem);
        printf("Ring size decreased to %d\n", ring_size);
    } else {
        printf("Ring size cannot be decreased\n");
    }
}

void create_producer() {
    if (num_prod_threads < max) {
        if (pthread_create(&producer_threads[++num_prod_threads - 1], NULL, producer, &num_prod_threads) != 0)
            perror("Producer_pthread_create error");
    }
}

void create_consumer() {
    if (num_cons_threads < max) {
        if (pthread_create(&consumer_threads[++num_cons_threads - 1], NULL, consumer, &num_cons_threads) != 0)
            perror("Consumer_pthread_create error");
    }
}

void stat() {
    pthread_mutex_lock(&ring_mutex);
    printf("---Stat---\nRing size: %d\nCount: %d\nAdded: %d\nGetted: %d\nProducers count: %d\nConsumers count: %d\n", ring_size, ring.count, ring.add, ring.get, num_prod_threads, num_cons_threads);
    pthread_mutex_unlock(&ring_mutex);
}

void quit() {
    while (num_prod_threads) {
        pthread_cancel(producer_threads[num_prod_threads - 1]);
        pthread_join(producer_threads[num_prod_threads-- - 1], NULL);
    };
    while (num_cons_threads) {
        pthread_cancel(consumer_threads[num_cons_threads - 1]);
        pthread_join(consumer_threads[num_cons_threads-- - 1], NULL);
    }
    wh = false;
} 

void init() {
    sem_init(&fill_sem, 0, ring_size);
    sem_init(&empty_sem, 0, 0);
    pthread_mutex_init(&ring_mutex, NULL);

    ring.head = 0;
    ring.tail = 0;
    ring.count = 0;
    ring.add = 0;
    ring.get = 0;
}

void dest() {
    sem_destroy(&fill_sem);
    sem_destroy(&empty_sem);
    pthread_mutex_destroy(&ring_mutex);
}

int main() {
    init();

    puts("P - create producer thread\nC - create consumer thread\n-(P/C) - end last created producer/consumer thread\n+/- - increase/decrease ring size\nS - show stat\nQ - end thread and exit\n");

    while (wh) {
        strcpy(input, "\0\0\0");
        scanf("%s", input);
        char key = input[0];
        if (key == '+')
            increase_size();
        else if (key == '-')
            decrease_size();
        if (key == 'p')
            create_producer();
        else if (key == 'c')
            create_consumer();
        if (key == 's')
            stat();
        else if (key == 'q')
            quit();
    }

    dest();

    return 0;
}