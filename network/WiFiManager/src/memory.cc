#include <exec/memory.h>
#include <proto/exec.h>

void *operator new(unsigned int sz)
{
    if (sz == 0)
        sz = 1;

    return AllocVec(sz, MEMF_CLEAR);
}

void *operator new[](unsigned int sz)
{
    if (sz == 0) 
        sz = 1;

    return AllocVec(sz, MEMF_CLEAR);
}

void operator delete(void *ptr) noexcept
{
    FreeVec(ptr);
}

void operator delete(void *ptr, unsigned int size) noexcept
{
    FreeVec(ptr);
}

void operator delete[](void *ptr) noexcept
{
    FreeVec(ptr);
}

void operator delete[](void *ptr, unsigned int size) noexcept
{
    FreeVec(ptr);
}
