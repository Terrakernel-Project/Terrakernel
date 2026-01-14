#include "ObjectManager.hpp"
#include <mem/mem.hpp>

namespace ObjMan {

static uint64_t objids = 0;
static uint32_t overflow_count = 0;

Handle* CreateNewHandle() {
    Handle* hptr = (Handle*)mem::usr::alloc((sizeof(Handle) + 0xFFF) / 0x1000);
    if (objids+1 == (uint64_t)-1) overflow_count = 1;
    hptr->ObjectID = objids++;
    return hptr;
}

void DestroyHandle(Handle* HandlePtr) {
    mem::usr::free(HandlePtr, (sizeof(Handle) + 0xFFF) / 0x1000);
}

bool ValidateID(uint64_t objid) {
    uint64_t objid_max = objids;
    if (overflow_count > 1) return true; // idk what to do now...
    return (objid < objid_max);
}

}