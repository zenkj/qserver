#ifndef __QS_COMMON__
#define __QS_COMMON__

#define QS_OK  0
#define QS_NOK 1

#define LVM_STATE_IDLE 0
#define LVM_STATE_GRAY 1
#define LVM_STATE_WORK 2

#define LVM_POOL_MAX 65536

#define MINI_WORKER_MAX 10000

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

struct lvm {
    lua_State *L;
    int state;
};

#endif // __QS_COMMON__

