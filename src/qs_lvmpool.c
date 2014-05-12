#include "qs_common.h"
#include "qs_lvmpool.h"
#include <sys/time.h>

void
lvm_pool_init(struct lvm_pool *pool) {
    pool->head = pool->tail = 0;
    pool->sleep = 0;
    pthread_mutex_init(&pool->mutex, NULL);
    pthread_cond_init(&pool->cond, NULL);
}

void
lvm_pool_destroy(struct lvm_pool *pool) {
    pthread_cond_broadcast(&pool->cond);
    if (pthread_mutex_lock(&pool->mutex) == 0) {
        while (pool->head != pool->tail) {
            int head = pool->head;
            pool->head = (pool->head + 1) % LVM_POOL_MAX;
            lua_close(pool->queue[head].L);
        }
        pthread_mutex_unlock(&pool->mutex);
    }

    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->cond);
}

int
lvm_pool_count(struct lvm_pool* pool) {
    int count = pool->tail - pool->head;
    count = count < 0 ? LVM_POOL_MAX + count : count;
    return count;
}

void
lvm_pool_add(struct lvm_pool* pool, struct lvm vm) {
    pool->queue[pool->tail] = vm;

    __sync_synchronize();

    pool->tail = (pool->tail + 1) % LVM_POOL_MAX;

    if (pool->sleep && lvm_pool_count(pool) > 10) {
        pthread_cond_signal(&pool->cond);
    }
}

int
lvm_pool_remove(struct lvm_pool* pool, struct lvm* vm) {
    if (pool->head == pool->tail)
        return QS_NOK;

    *vm = pool->queue[pool->head];
    pool->queue[pool->head].L = NULL;
    pool->head = (pool->head + 1) % LVM_POOL_MAX;
    return QS_OK;
}

int
lvm_pool_remove_wait(struct lvm_pool* pool, struct lvm* vm, int seconds) {
    int result = lvm_pool_remove(pool, vm);
    if (result == QS_OK)
        return result;

    if (pthread_mutex_lock(&pool->mutex) == 0) {
        struct timeval now;
        struct timespec timeout;

        result = lvm_pool_remove(pool, vm);
        if(result == QS_OK) {
            pthread_mutex_unlock(&pool->mutex);
            return result;
        }
        gettimeofday(&now,NULL);
        timeout.tv_sec = now.tv_sec + seconds;
        timeout.tv_nsec = now.tv_usec * 1000;
        pool->sleep = 1;
        pthread_cond_timedwait(&pool->cond, &pool->mutex, &timeout);
        pool->sleep = 0;
        pthread_mutex_unlock(&pool->mutex);
    }

    return lvm_pool_remove(pool, vm);
}


