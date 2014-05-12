#include "qs_common.h"
#include "qs_lvmpool.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define LUA_ENTRY "doit"
#define LUA_FILE  "src/worker.lua"

struct qs_userdata {
    int  n;
    int  total;
    int  success;
    int  failed;
    char buf[128];
};

struct qs_server {
    struct lvm_pool    graypool;
    struct lvm_pool    idlepool;
    struct qs_userdata udlist [MINI_WORKER_MAX];

    int                work_count;

    int                lvm_count;
    volatile int       finish;
};

static int
inccount (lua_State *L) {
    struct qs_userdata *ud;
    void *_ud = lua_touserdata(L, 1);
    if (_ud == NULL)
        return 0;
    ud = (struct qs_userdata*)_ud;
    ud->total++;
    return 0;
}

static struct qs_userdata*
init_miniworker(struct qs_server *server, int workno) {
    struct qs_userdata* ud = &server->udlist[workno];
    ud->n = rand();
    return ud;
}

static struct lvm
new_lvm(){
    struct lvm vm;
    int result;
    vm.state = LVM_STATE_IDLE;
    vm.L = luaL_newstate();
    if (vm.L == NULL) {
        fprintf(stderr, "create lua vm failed\n");
        exit(1);
    }

    luaL_openlibs(vm.L);

    lua_pushcfunction(vm.L, inccount);
    lua_setglobal(vm.L, "inccount");

    result = luaL_dofile(vm.L, LUA_FILE);
    if (result != LUA_OK) {
        fprintf (stderr, "load lua file %s failed\n",LUA_FILE);
        exit(1);
    }

    lua_getglobal(vm.L, LUA_ENTRY);
    if (lua_type(vm.L, -1) != LUA_TFUNCTION) {
        fprintf(stderr, "lua entry function %s is not defined in %s\n", LUA_ENTRY, LUA_FILE);
        exit(1);
    }

    lua_pop(vm.L,1);

    lua_gc(vm.L, LUA_GCSTOP, 0);

    return vm;
}

static void*
work(void *ud) {
    struct qs_server *server = (struct qs_server*) ud;

    for (;;) {
        struct lvm vm;
        struct qs_userdata *ud;
        int workno;
        int result;
        if (server->finish) break;

        workno = rand() % MINI_WORKER_MAX;

        if (lvm_pool_remove(&server->idlepool, &vm) == QS_NOK) {
            if(server->lvm_count == LVM_POOL_MAX) {
                fprintf(stderr, "too much lua vm{%d) is created, rest for sometime\n", server->lvm_count);
                usleep(500*1000);
                continue;
            }
            vm = new_lvm();
            server->lvm_count++;
        }
        vm.state = LVM_STATE_WORK;

        ud = init_miniworker(server, workno);

        lua_getglobal(vm.L, "doit");
        lua_pushlightuserdata(vm.L, ud);
        lua_pushinteger(vm.L, server->work_count);

        result = lua_pcall(vm.L, 2, 0, 0);

        if (result != LUA_OK) {
            ud->failed++;
        } else {
            ud->success++;
        }

        vm.state = LVM_STATE_GRAY;
        lvm_pool_add(&server->graypool, vm);
    }
}

static void*
gc(void* ud) {
    struct qs_server *server = (struct qs_server*) ud;
    int finish_delay = 0;

    for (;;) {
        for (;;) {
            struct lvm vm;
            if (lvm_pool_remove_wait(&server->graypool, &vm, 1) == QS_NOK)
                break;
            lua_gc(vm.L, LUA_GCCOLLECT, 0);
            vm.state = LVM_STATE_IDLE;
            lvm_pool_add(&server->idlepool, vm);
        }

        if (server->finish && finish_delay == 0) {
            finish_delay = 3;
        }

        if (finish_delay > 0) {
            finish_delay--;
            if (finish_delay == 0)
                break;
        }
    }
}

static void
report (struct qs_server* server) {
    int gray, idle;
    int total_req, succ_req, fail_req;
    int i;

    gray = idle = 0;
    total_req = succ_req = fail_req = 0;

    gray = lvm_pool_count(&server->graypool);
    idle = lvm_pool_count(&server->idlepool);
    for (i=0; i<MINI_WORKER_MAX; i++) {
        total_req += server->udlist[i].total;
        succ_req += server->udlist[i].success;
        fail_req += server->udlist[i].failed;
    }

    printf ("gray vm: %d, idle vm: %d;\ttotal req: %d, success: %d, failed: %d\tvm count: %d\n",
            gray, idle, total_req, succ_req, fail_req, server->lvm_count);
}

static void
period_report (struct qs_server* server, int count) {
    int i;

    for (i=0; i<count; i++) {
        sleep(1);
        report (server);
    }
}

static void
new_thread(pthread_t* pid, void*(*f)(void*), void* ud) {
    if (pthread_create(pid, NULL, f, ud) != 0) {
        fprintf(stderr, "create thread failed\n");
        exit(1);
    }
}

static void
join_thread(pthread_t pid) {
    pthread_join(pid, NULL);
}

int
main (int argc, char* argv[]) {
    pthread_t workpid, gcpid;
    int period = 5;
    int work_count = 10;
    struct qs_server *server;

    if (argc == 2 && strcmp(argv[1], "-h") == 0) {
        printf("\nUsage:\n");
        printf("    %s [run_duration [lua_work_load]]\n\n", argv[0]);
        return 0;
    }

    if (argc >= 2) {
        int n = atoi(argv[1]);
        if (n < 1) n = 1;
        period = n;
    }

    if (argc >= 3) {
        int n = atoi(argv[2]);
        if (n < 1) n = 1;
        work_count = n;
    }
    
    server = (struct qs_server*) malloc(sizeof(struct qs_server));
    if (server == NULL) {
        fprintf (stderr, "no enough memory\n");
        exit(1);
    }

    memset (server, 0, sizeof(struct qs_server));

    server->work_count = work_count;

    lvm_pool_init(&server->graypool);
    lvm_pool_init(&server->idlepool);

    new_thread (&workpid, work, server);
    new_thread (&gcpid, gc, server);

    period_report (server, period);

    server->finish = 1;

    join_thread (workpid);
    join_thread (gcpid);

    report(server);
    printf("\n\n");

    lvm_pool_destroy(&server->graypool);
    lvm_pool_destroy(&server->idlepool);

    free(server);

    return 0;
}
