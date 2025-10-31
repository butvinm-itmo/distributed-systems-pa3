#ifndef __IFMO_DISTRIBUTED_CLASS_WORKER__H
#define __IFMO_DISTRIBUTED_CLASS_WORKER__H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef int8_t worker_id;

typedef struct {
    int read_fd;
    int write_fd;
} Channel;

typedef struct {
    worker_id id;
    Channel* chs;
    worker_id nbr_count;
    FILE* events_log;
} Worker;

int init_duplex_channel(Channel* ch_0, Channel* ch_1, FILE* pipes_log);

void deinit_unused_channels(Worker* s, Worker* workers, FILE* pipes_log);

void init_worker(Worker* s, worker_id id, worker_id nbr_count, FILE* events_log, FILE* pipes_log);

int init_workers(Worker* workers, worker_id nbr_count, FILE* events_log, FILE* pipes_log);

void deinit_workers(Worker* s, Worker* workers, FILE* pipes_log);

#endif // __IFMO_DISTRIBUTED_CLASS_WORKER__H
