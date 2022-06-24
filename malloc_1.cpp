#include "malloc_1.h"

void *malloc_1::smalloc(size_t size) {
    if(size == 0 || size > (long) pow(10, 8)) return nullptr;

    void *program_break = sbrk((intptr_t) size);
    if(program_break == (void *)-1){
        return nullptr;
    }

    return program_break;
}
