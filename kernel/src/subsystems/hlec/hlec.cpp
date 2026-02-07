#include "hlec.hpp"
#include <cstring>
#include <mem/mem.hpp>

struct __private_event_context {
	int64_t ev_id;
	char* ev_name;
	int64_t(*event_callback)(int,EventArguments*);
	bool quiet;

	__private_event_context* ev_next;
};

#define EV_BEGIN __private_event_context* ev = (__private_event_context*)evctx;

__private_event_context* first_event;
__private_event_context* last_event;

__private_event_context* alloc_event() {
    __private_event_context* new_ev = (__private_event_context*)mem::heap::malloc(sizeof(__private_event_context));
    if (!new_ev) return nullptr;

    new_ev->ev_next = nullptr;
    new_ev->ev_name = nullptr;
    new_ev->event_callback = nullptr;
    new_ev->quiet = true;

    if (!first_event) {
        first_event = new_ev;
        last_event = new_ev;
    } else {
        last_event->ev_next = new_ev;
        last_event = new_ev;
    }

    return new_ev;
}

__private_event_context* get_prev_event_for_id(int64_t event_id) {
    __private_event_context* curr = first_event;
    while (curr && curr->ev_next) {
        if (curr->ev_next->ev_id == event_id) return curr;
        curr = curr->ev_next;
    }
    return nullptr;
}

__private_event_context* get_event_for_id(int64_t event_id) {
	__private_event_context* curr = first_event;
	while (curr) {
		if (curr->ev_id == event_id) return curr;
		curr = curr->ev_next;
	}

	return nullptr;
}

namespace subsystems::hlec {

/* == managing events ================================ */

int64_t last_id = 0;
event_context* new_event(const char* __restrict name) {
	__private_event_context* ev = alloc_event();
	if (!ev) return nullptr;

	ev->ev_id = last_id++;
	ev->ev_name = strdup(name);
	ev->quiet = true;

	return (event_context*)ev;
}

void remove_event(event_context* evctx) {
	EV_BEGIN /* already have ev var now, same for rest */
	if (!ev) return;

	__private_event_context* prev = get_prev_event_for_id(ev->ev_id);
	__private_event_context* node = get_event_for_id(ev->ev_id);
	if (!node) return;

	if (prev) {
		prev->ev_next = node->ev_next;
	} else {
		first_event = node->ev_next;
	}

	node->ev_next = nullptr;
	mem::heap::free(node->ev_name);
	mem::heap::free(node);
}

void remove_event(int64_t event_id) {
	__private_event_context* prev = get_prev_event_for_id(event_id);
	__private_event_context* node = get_event_for_id(event_id);
	if (!node) return;

	if (prev) {
		prev->ev_next = node->ev_next;
	} else {
		first_event = node->ev_next;
	}

	node->ev_next = nullptr;
	mem::heap::free(node->ev_name);
	mem::heap::free(node);
}

bool register_event_callback(event_context* evctx, int64_t(*event_callback)(int,EventArguments*)) {
    EV_BEGIN
    if (!ev) return false;

    ev->event_callback = event_callback;
    return true;
}

bool register_event_callback(int64_t event_id, int64_t(*event_callback)(int,EventArguments*)) {
	__private_event_context* ev = get_event_for_id(event_id);
	if (!ev) return false;

	ev->event_callback = event_callback;
	return true;
}

/* == obatining event ids ============================ */

int64_t obtain_event_id(event_context* evctx) {
	EV_BEGIN
	if (!ev) return -1;
	return ev->ev_id;
}

/* == raising events ================================= */

int64_t raise_event(int64_t event_id, int argc, EventArguments* args) {
	__private_event_context* ev = get_event_for_id(event_id);
	if (!ev) return -1;

	if (ev->quiet) return -1;

	if (!ev->event_callback) return -1;

	return ev->event_callback(argc, args);
}

int64_t raise_event(event_context* evctx, int argc, EventArguments* args) {
	EV_BEGIN
	if (!ev) return -1;

	if (ev->quiet) return -1;

	if (!ev->event_callback) return -1;

	return ev->event_callback(argc, args);
}

/* == setting event quiet-ness ======================= */

void set_event_quiet(int64_t event_id) {
	__private_event_context* ev = get_event_for_id(event_id);
	if (!ev) return;

	ev->quiet = true;
}

void set_event_quiet(event_context* evctx) {
	EV_BEGIN
	if (!ev) return;

	ev->quiet = true;
}

/* == unsetting event quiet-ness ===================== */

void unset_event_quiet(int64_t event_id) {
	__private_event_context* ev = get_event_for_id(event_id);
	if (!ev) return;
	
	ev->quiet = false;
}

void unset_event_quiet(event_context* evctx) {
	EV_BEGIN
	if (!ev) return;
	
	ev->quiet = false;
}

}
