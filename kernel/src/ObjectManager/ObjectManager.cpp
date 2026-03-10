#include "ObjectManager.hpp"
#include <mem/mem.hpp>
#include <cstdio>

struct handle_allocation {
	Handle* handle;
	handle_allocation* next;
};

handle_allocation* allocations;
uint64_t num_allocations = 0;

handle_allocation* alloc_handle_allocation() {
    handle_allocation* new_alloc = (handle_allocation*)mem::heap::malloc(sizeof(handle_allocation));
    if (!new_alloc) return nullptr;
    
    new_alloc->handle = nullptr;
    new_alloc->next = nullptr;
    
    if (!allocations) {
        allocations = new_alloc;
    } else {
        handle_allocation* cursor = allocations;
        while (cursor->next) cursor = cursor->next;
        cursor->next = new_alloc;
    }
    
    num_allocations++;
    
    return new_alloc;
}

namespace ObjMan {

static uint64_t objids = 0;
static uint32_t overflow_count = 0;

Handle* CreateNewHandle() {
    Handle* hptr = (Handle*)mem::usr::alloc((sizeof(Handle) + 0xFFF) / 0x1000);
    handle_allocation* allocation = alloc_handle_allocation();
    
    if (!allocation) {
    	mem::usr::free(hptr, (sizeof(Handle) + 0xFFF) / 0x1000);
    	return nullptr;
    }
    
    allocation->handle = hptr;
    if (objids+1 == (uint64_t)-1) overflow_count++;
    hptr->ObjectID = objids++;
    hptr->GlobalHd = hptr->ObjectID; // currently lets just use the object ID
    
    return hptr;
}

void DestroyHandle(Handle* HandlePtr) {
    handle_allocation* prev = nullptr;
    handle_allocation* cursor = allocations;
    
    for (uint64_t i = 0; i < num_allocations; i++) {
    	if (cursor->handle == HandlePtr) break;
    	else {
    		prev = cursor;
    		cursor = cursor->next;
    	}
    }
    
    handle_allocation* next = cursor->next;
    
    if (!prev && !next) {
    	mem::heap::free(cursor);
    	allocations = nullptr;
    } else if (!prev) {
    	mem::heap::free(cursor);
    	allocations = next;
    } else if (!next) {
    	prev->next = nullptr;
    	mem::heap::free(cursor);
    } else {
    	prev->next = next;
    	mem::heap::free(cursor);
    }
    
    num_allocations--;
    mem::usr::free(HandlePtr, (sizeof(Handle) + 0xFFF) / 0x1000);
}

bool ValidateID(uint64_t objid) {
    uint64_t objid_max = objids;
    if (overflow_count > 1) return true; // idk what to do now...
    return (objid < objid_max);
}

Handle* GetHandleFromHd(uint64_t hd) {
    handle_allocation* cursor = allocations;
    for (uint64_t i = 0; i < num_allocations; i++) {
    	if (cursor->handle && cursor->handle->GlobalHd == hd) return cursor->handle;
    	cursor = cursor->next;
    }
    return nullptr;
}

}
