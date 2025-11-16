#ifndef __IFMO_DISTRIBUTED_CLASS_LAMPORT__H
#define __IFMO_DISTRIBUTED_CLASS_LAMPORT__H

#include "ipc.h"

void increment_lamport_time(void);

void update_lamport_time(timestamp_t received_time);

#endif // __IFMO_DISTRIBUTED_CLASS_LAMPORT__H
