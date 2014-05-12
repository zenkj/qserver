#ifndef __QS_LVMPOOL_H__
#define __QS_LVMPOOL_H__

#include <pthread.h>

struct lvm_pool {
    int head;
    //tail is volatile because of multithread access
    volatile int tail;
    int sleep;
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    struct lvm queue[LVM_POOL_MAX];
};

void lvm_pool_init(struct lvm_pool *pool);
void lvm_pool_destroy(struct lvm_pool *pool);
int  lvm_pool_count(struct lvm_pool *pool);
void lvm_pool_add(struct lvm_pool *pool, struct lvm vm);
int  lvm_pool_remove(struct lvm_pool *pool, struct lvm *vm);
int  lvm_pool_remove_wait(struct lvm_pool *pool, struct lvm *vm, int seconds);

#endif // __QS_LVMPOOL_H__
