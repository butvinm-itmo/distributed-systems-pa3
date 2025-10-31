#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "worker.h"

int _set_non_block_fd(int fd, FILE* pipes_log) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        fprintf(pipes_log, "Failed to get fd controls: %s", strerror(errno));
        return -1;
    }
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) == -1) {
        fprintf(pipes_log, "Failed to set fd controls: %s", strerror(errno));
        return -1;
    }
    return fd;
}

int init_duplex_channel(Channel* ch_0, Channel* ch_1, FILE* pipes_log) {
    int fildes[2];

    if (pipe(fildes) == -1) return -1;
    ch_0->read_fd = _set_non_block_fd(fildes[0], pipes_log);
    ch_1->write_fd = _set_non_block_fd(fildes[1], pipes_log);

    if (pipe(fildes) == -1) return -1;
    ch_1->read_fd = _set_non_block_fd(fildes[0], pipes_log);
    ch_0->write_fd = _set_non_block_fd(fildes[1], pipes_log);

    return 0;
}

void deinit_unused_channels(Worker* s, Worker* workers, FILE* pipes_log) {
    for (worker_id nbr_id = 0; nbr_id < s->nbr_count + 1; nbr_id++) {
        if (nbr_id == s->id) continue;

        Worker* nbr = &workers[nbr_id];
        for (worker_id other_nbr_id = 0; other_nbr_id < s->nbr_count + 1; other_nbr_id++) {
            if (other_nbr_id == nbr->id) continue;
            close(nbr->chs[other_nbr_id].read_fd);
            close(nbr->chs[other_nbr_id].write_fd);
            fprintf(pipes_log, "[deinit_unused_channels] Worker %d closes semi-duplex channel between processes %d and %d (read_fd=%d write_fd=%d)\n", s->id, nbr_id, other_nbr_id, nbr->chs[other_nbr_id].read_fd, nbr->chs[other_nbr_id].write_fd);
            fflush(pipes_log);
        }
    }
}

void init_worker(Worker* s, worker_id id, worker_id nbr_count, FILE* events_log, FILE* pipes_log) {
    s->id = id;
    s->nbr_count = nbr_count;
    s->chs = calloc(nbr_count + 1, sizeof(Channel));
    s->events_log = events_log;
}

void deinit_workers(Worker* s, Worker* workers, FILE* pipes_log) {
    if (workers != NULL) {
        for (worker_id nbr_id = 0; nbr_id < s->nbr_count + 1; nbr_id++) {
            if (nbr_id == s->id) continue;
            close(s->chs[nbr_id].read_fd);
            close(s->chs[nbr_id].write_fd);
            fprintf(pipes_log, "[deinit_workers] Worker %d closes semi-duplex channel between processes %d and %d (read_fd=%d write_fd=%d)\n", s->id, s->id, nbr_id, s->chs[nbr_id].read_fd, s->chs[nbr_id].write_fd);
            fflush(pipes_log);
        }
        for (worker_id self_id = 0; self_id < s->nbr_count + 1; self_id++) free(workers[self_id].chs);
        free(workers);
    }
}

int init_workers(Worker* workers, worker_id nbr_count, FILE* events_log, FILE* pipes_log) {
    for (worker_id self_id = 0; self_id < nbr_count + 1; self_id++) init_worker(&workers[self_id], self_id, nbr_count, events_log, pipes_log);

    for (worker_id self_id = 0; self_id < nbr_count + 1; self_id++) {
        for (worker_id nbr_id = self_id + 1; nbr_id < nbr_count + 1; nbr_id++) {
            if (init_duplex_channel(&workers[self_id].chs[nbr_id], &workers[nbr_id].chs[self_id], pipes_log) != 0) {
                fprintf(stderr, "Failed to initialize a duplex channel between [%d] and [%d]: %s\n", self_id, nbr_id, strerror(errno));
                return 1;
            }
            fprintf(pipes_log, "[init_workers] Open duplex channel between processes %d and %d\n", self_id, nbr_id);
            fprintf(pipes_log, "[init_workers] Worker %d read_fd=%d write_fd=%d\n", self_id, workers[self_id].chs[nbr_id].read_fd, workers[self_id].chs[nbr_id].write_fd);
            fprintf(pipes_log, "[init_workers] Worker %d read_fd=%d write_fd=%d\n", nbr_id, workers[nbr_id].chs[self_id].read_fd, workers[nbr_id].chs[self_id].write_fd);
            fflush(pipes_log);
        }
    }
    return 0;
}
