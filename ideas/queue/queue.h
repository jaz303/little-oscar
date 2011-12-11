#ifndef __QUEUE_H__
#define __QUEUE_H__

#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>

//
// compute difference (t2 - t1) of two `struct timeval *` and store in `out`.
// out->tv_usec is always +ve, so a value of -1.5s would be represented as:
// tv_sec = -2, tv_usec = 500000. negative results can therefore be identified
// by checking sign of out->tv_sec only.

#define OSC_TIMEVAL_DIFF(t1, t2, out)                       \
    do {                                                    \
        (out)->tv_sec = (t2)->tv_sec - (t1)->tv_sec;        \
        (out)->tv_usec = (t2)->tv_usec - (t1)->tv_usec;     \
        if ((out)->tv_usec < 0) {                           \
            (out)->tv_sec--;                                \
            (out)->tv_usec += 1000000;                      \
        }                                                   \
    } while (0)
        
//
// Time abstraction

typedef struct timeval osc_time_t;

#define OSC_TIME_SET_NOW(var)       gettimeofday(&var)

// create a named variable containing the current time on the stack
#define OSC_TIME_MK_NOW(var)        osc_time_t var; \
                                    OSC_TIME_SET_NOW(var)

// compare two time values with the given operator
#define OSC_TIME_CMP(i1, OP, i2)    (((i1).tv_sec == (i2).tv_sec) ?       \
                                     ((i1).tv_usec OP (i2).tv_usec) :     \
                                     ((i1).tv_sec OP (i2).tv_sec))
                                 
// test two time values for equality
#define OSC_TIME_EQ(i1, i2)         OSC_TIME_CMP(i1, ==, i2)

// compute i2 - i1 and store result in out (a `struct timeval *`)
// note: struct timeval is used for output regardless of time representation
// used elsewhere
#define OSC_TIME_DIFF(i1, i2, out)  OSC_TIMEVAL_DIFF(&i1, &i2, out)

//
//

typedef struct osc_msg {
    osc_time_t      due;
    int             value;
} osc_msg_t;

typedef struct osc_msg_queue {
    osc_msg_t           **heap;
    size_t              n_items;
    size_t              c_items;
    int                 flags;
    pthread_mutex_t     lock;
    pthread_cond_t      cond;
} osc_msg_queue_t;

enum {
    OSC_QUEUE_GROWABLE      = 1
};

int         osc_msg_queue_init(osc_msg_queue_t *queue, int initial_capacity, int flags);
int         osc_msg_queue_teardown(osc_msg_queue_t *queue);

//
// 

size_t      osc_msg_queue_size(osc_msg_queue_t *queue);
int         osc_msg_queue_add(osc_msg_queue_t *queue, osc_msg_t *msg);
osc_msg_t*  osc_msg_queue_remove(osc_msg_queue_t *queue);

//
// Locking

void        osc_msg_queue_lock(osc_msg_queue_t *queue);
void        osc_msg_queue_unlock(osc_msg_queue_t *queue);

//
// Thread-safe wrappers

// add immediately, fails if no space
int         osc_msg_queue_add_s(osc_msg_queue_t *queue, osc_msg_t *msg);

// remove head of queue immediately, NULL if queue is empty
osc_msg_t*  osc_msg_queue_remove_s(osc_msg_queue_t *queue);

// if head of queue is due, remove and return, otherwise return NULL
osc_msg_t*  osc_msg_queue_remove_due_s(osc_msg_queue_t *queue, struct timeval *threshold);

// wait for item to become available, then return it
osc_msg_t*  osc_msg_queue_take_s(osc_msg_queue_t *queue);

// wait for item to become both available and due, then return it
osc_msg_t*  osc_msg_queue_take_due_s(osc_msg_queue_t *queue, struct timeval *threshold);

#endif