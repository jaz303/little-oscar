#include "queue.h"

#include <sched.h>

// some thoughts on yielding:
// http://www.technovelty.org/code/c/sched_yield.html

#define HEAP_PARENT(ix)         ((ix-1)/2)
#define HEAP_LEFT_IX(ix)        ((2*ix)+1)
#define HEAP_RIGHT_IX(ix)       ((2*ix)+2)
#define HEAP_MSG_P(i)           ((i)->due)
#define HEAP_P(q, ix)           (HEAP_MSG_P(q->heap[ix]))
#define HEAP_SIZE(q)            (q->c_items)
#define HEAP_ROOT(q)            (q->heap[0])

#define LOCK(q)                 (pthread_mutex_lock(&q->lock))
#define UNLOCK(q)               (pthread_mutex_unlock(&q->lock))

int osc_msg_queue_init(osc_msg_queue_t *queue, int initial_capacity, int flags) {
    queue->heap = malloc(sizeof(osc_msg_t) * initial_capacity);
    if (!queue->heap) {
        return 0;
    }
    queue->n_items = initial_capacity;
    queue->c_items = 0;
    queue->flags = flags;
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->cond, NULL);
    return 1;
}

int osc_msg_queue_teardown(osc_msg_queue_t *queue) {
    if (queue->heap) free(queue->heap);
    return 1;
}

//
// Default (non-threadsafe) queue functions

size_t osc_msg_queue_size(osc_msg_queue_t *queue) {
    return HEAP_SIZE(queue);
}

int osc_msg_queue_add(osc_msg_queue_t *queue, osc_msg_t *msg) {
    
    if (queue->c_items == queue->n_items) {
        if (!(queue->flags & OSC_QUEUE_GROWABLE)) {
            return 0;
        }
        queue->n_items *= 2;
        queue->heap = realloc(queue->heap, sizeof(osc_msg_t) * queue->n_items);
        if (!queue->heap) {
            queue->n_items /= 2;
            return 0;
        }
    }
    
    size_t ix       = queue->c_items++;
    size_t parent   = HEAP_PARENT(ix);
    
    queue->heap[ix] = msg;
    
    while ((ix > 0) && OSC_TIME_CMP(HEAP_P(queue, parent), >, HEAP_MSG_P(msg))) {
        osc_msg_t *tmp = queue->heap[parent];
        queue->heap[parent] = queue->heap[ix];
        queue->heap[ix] = tmp;
        ix = parent;
        parent = HEAP_PARENT(ix);
    }
    
    return 1;
    
}

osc_msg_t* osc_msg_queue_remove(osc_msg_queue_t *queue) {
    
    if (HEAP_SIZE(queue) == 0) {
        return NULL;
    }
    
    osc_msg_t *out = HEAP_ROOT(queue);
    queue->heap[0] = queue->heap[--queue->c_items];
    
    int ix = 0;
    
    while (1) {
        size_t  left_ix     = HEAP_LEFT_IX(ix),
                right_ix    = HEAP_RIGHT_IX(ix),
                smallest_ix = ix;

        if (left_ix < HEAP_SIZE(queue) && OSC_TIME_CMP(HEAP_P(queue, left_ix),
                                                       <,
                                                       HEAP_P(queue, smallest_ix))) {
            smallest_ix = left_ix;
        }

        if (right_ix < HEAP_SIZE(queue) && OSC_TIME_CMP(HEAP_P(queue, right_ix),
                                                        <,
                                                        HEAP_P(queue, smallest_ix))) {
            smallest_ix = right_ix;
        }

        if (smallest_ix != ix) {
            osc_msg_t *tmp = queue->heap[ix];
            queue->heap[ix] = queue->heap[smallest_ix];
            queue->heap[smallest_ix] = tmp;
            ix = smallest_ix;
        } else {
            break;
        }
    }
    
    return out;

}

//
// Locking

void osc_msg_queue_lock(osc_msg_queue_t *queue) {
    LOCK(queue);
}

void osc_msg_queue_unlock(osc_msg_queue_t *queue) {
    UNLOCK(queue);
}

//
// Thread-safe functions

int osc_msg_queue_add_s(osc_msg_queue_t *queue, osc_msg_t *msg) {
    LOCK(queue);
    int was_empty = (HEAP_SIZE(queue) == 0);
    int ret = osc_msg_queue_add(queue, msg);
    if (ret && was_empty) {
        pthread_cond_signal(&queue->cond);
    }
    UNLOCK(queue);
    return ret;
}

osc_msg_t* osc_msg_queue_remove_s(osc_msg_queue_t *queue) {
    LOCK(queue);
    osc_msg_t *msg = osc_msg_queue_remove(queue);
    UNLOCK(queue);
    return msg;
}

inline static int _osc_is_msg_due(osc_msg_t *msg, struct timeval *threshold, struct timeval *diff) {
    OSC_TIME_MK_NOW(now);
    OSC_TIME_DIFF(now, HEAP_MSG_P(msg), diff);
    if (diff->tv_sec < 0) {
        return 1;
    } else if (threshold == NULL) {
        return diff->tv_sec == 0 && diff->tv_usec == 0;
    } else {
        OSC_TIMEVAL_DIFF(threshold, diff, diff);
        return (diff->tv_sec < 0 || (diff->tv_sec == 0 && diff->tv_usec == 0));
    }
}

osc_msg_t* osc_msg_queue_remove_due_s(osc_msg_queue_t *queue, struct timeval *threshold) {
    osc_msg_t *msg = NULL;
    LOCK(queue);
    if (HEAP_SIZE(queue) > 0) {
        struct timeval diff;
        if (_osc_is_msg_due(HEAP_ROOT(queue), threshold, &diff)) {
            msg = osc_msg_queue_remove(queue);
        }
    }
    UNLOCK(queue);
    return msg;
}

osc_msg_t* osc_msg_queue_take_s(osc_msg_queue_t *queue) {
    osc_msg_t *msg = NULL;
    LOCK(queue);
    while (1) {
        if (HEAP_SIZE(queue) == 0) {
            pthread_cond_wait(&queue->cond, &queue->lock);
        } else {
            msg = osc_msg_queue_remove(queue);
            break;
        }
    }
    UNLOCK(queue);
    return msg;
}

osc_msg_t* osc_msg_queue_take_due_s(osc_msg_queue_t *queue, struct timeval *threshold) {
    osc_msg_t *msg = NULL;
    LOCK(queue);
    while (1) {
        if (HEAP_SIZE(queue) == 0) {
            pthread_cond_wait(&queue->cond, &queue->lock);
        } else {
            struct timeval diff;
            if (_osc_is_msg_due(HEAP_ROOT(queue), threshold, &diff)) {
                msg = osc_msg_queue_remove(queue);
                break;
            } else {
                // TODO: need to replace this with some better time-control
                //sched_yield();
                
                UNLOCK(queue);
                usleep(1000); // 1ms
                LOCK(queue);
                
                // if (threshold == NULL) {
                //     UNLOCK(queue);
                //     // sleep for the min of (diff, queue max sleep time)
                //     LOCK(queue);
                // } else {
                //     UNLOCK(queue);
                //     // 
                //     LOCK(queue);
                // }
            }
        }
    }
    UNLOCK(queue);
    return msg;
}
