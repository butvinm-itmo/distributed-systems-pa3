#include "banking.h"

static timestamp_t lamport_clock = 0;

timestamp_t get_lamport_time(void) {
    return lamport_clock;
}

void increment_lamport_time(void) {
    lamport_clock++;
}

void update_lamport_time(timestamp_t received_time) {
    if (received_time > lamport_clock) {
        lamport_clock = received_time;
    }
    lamport_clock++;
}
