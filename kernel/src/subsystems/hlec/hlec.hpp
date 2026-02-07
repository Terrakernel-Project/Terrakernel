#ifndef HLEC_HPP
#define HLEC_HPP 1

#include <cstdint>

#ifdef __cplusplus
#define NO_MANGLE extern "C"
#else
#define NO_MANGLE
#endif

#define MAX_ARGS 10

struct EventArguments {
    uint64_t arg[MAX_ARGS];
    int num_args;
};

struct event_context;

/* Event callback function */
/*
	NO_MANGLE int64_t <name>(int argc, EventArguments* args);
*/

namespace subsystems::hlec {

event_context* new_event(const char* __restrict name);

void remove_event(event_context* evctx);
void remove_event(int64_t event_id);

bool register_event_callback(int64_t event_id, int64_t(*event_callback)(int,EventArguments*));
bool register_event_callback(event_context* evctx, int64_t(*event_callback)(int,EventArguments*));

int64_t obtain_event_id(event_context* evctx);

int64_t raise_event(int64_t event_id, int argc, EventArguments* args);
int64_t raise_event(event_context* evctx, int argc, EventArguments* args);

// event wont be raised while quiet
void set_event_quiet(int64_t event_id);
void set_event_quiet(event_context* evctx);

// event could be raised
void unset_event_quiet(int64_t event_id);
void unset_event_quiet(event_context* evctx);


}

#endif
