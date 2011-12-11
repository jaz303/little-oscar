#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include <time.h>

#include "queue.h"

osc_msg_t items[128];

void* produce(void *userdata) {
    OSC_TIME_MK_NOW(now);
    now.tv_sec += 1;
    now.tv_usec = 0;
    
    osc_msg_queue_t *queue = (osc_msg_queue_t*)userdata;
    int i;
    for (i = 0; i < 128; i++) {
        now.tv_usec += 100000;
        if (now.tv_usec == 1000000) {
            now.tv_sec += 1;
            now.tv_usec = 0;
        }
        items[i].due = now;
        items[i].value = i;
        osc_msg_queue_add_s(queue, &items[i]);
    }
}

void* consume(void *userdata) {
    osc_msg_queue_t *queue = (osc_msg_queue_t*)userdata;
    while (1) {
        osc_msg_t *msg = osc_msg_queue_take_due_s(queue, NULL);
        printf("taken: %d\n", (int) msg->value);
        fflush(stdout);
    }
}

int main(int argc, char *argv[]) {
    
    osc_msg_queue_t queue;
    osc_msg_queue_init(&queue, 32, OSC_QUEUE_GROWABLE);
    
    pthread_attr_t thread_attr;
    pthread_attr_init(&thread_attr);
    pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE);
    
    pthread_t producer, c1, c2, c3;
    
    printf("starting a queue...\n");
    
    void *exit_status;
    
    pthread_create(&producer, &thread_attr, produce, (void*)&queue);
    pthread_create(&c1, &thread_attr, consume, (void*)&queue);
    pthread_create(&c2, &thread_attr, consume, (void*)&queue);
    pthread_create(&c3, &thread_attr, consume, (void*)&queue);
    
    pthread_join(producer, &exit_status);
    pthread_join(c1, &exit_status);
    pthread_join(c2, &exit_status);
    pthread_join(c3, &exit_status);
    
    return 0;

}