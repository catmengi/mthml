#pragma once
#include "handle_heap/mm_handles.h"

#define HSTACK_MAX 256

typedef struct{
    mm_handle hstack[HSTACK_MAX];
    int pos;
}hstack;

static inline int hstack_push(mm_handle push, hstack* hstack){
    if(hstack->pos < HSTACK_MAX){
        hstack->hstack[hstack->pos++] = push;
        return 0;
    }
    return 1;
}

static inline mm_handle hstack_peek(hstack* hstack){
    if(hstack->pos >= 0){
        return hstack->hstack[hstack->pos - 1];
    }
    return (mm_handle){0};
}

static inline mm_handle hstack_pop(hstack* hstack){
    if (hstack->pos > 0) {
        return hstack->hstack[--hstack->pos];
    }
    return (mm_handle){0};
}
