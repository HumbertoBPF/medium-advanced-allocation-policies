# include "memoryman.h"

int main() {
    void *ptr0 = customMalloc(200);
    traverseFreeList();

    void *ptr1 = customMalloc(200);
    traverseFreeList();

    void *ptr2 = customMalloc(1000);
    traverseFreeList();

    customFree(ptr1);
    traverseFreeList();

    customFree(ptr0);
    traverseFreeList();

    customFree(ptr2);
    traverseFreeList();
    
    return 0;
}