
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "banking.h"
#include "common.h"
#include "ipc.h"
#include "pa1.h"
#include "worker.h"

#define defer_return(r) \
    do {                \
        result = r;     \
        goto defer;     \
    } while (0)

void transfer(void* parent_data, local_id src, local_id dst, balance_t amount) {
    // student, please implement me
}

int run_worker(Worker* s) {
    Message msg;
    size_t started = 0;
    size_t done = 0;

    msg = (Message) { .s_header = { .s_magic = MESSAGE_MAGIC, .s_type = STARTED, .s_local_time = s->id } };
    sprintf(msg.s_payload, log_started_fmt, s->id, getpid(), getppid());
    msg.s_header.s_payload_len = strlen(msg.s_payload);
    if (send_multicast(s, &msg) != 0) {
        fprintf(s->events_log, "Process %1d failed to multicast STARTED message: %s\n", s->id, strerror(errno));
        fprintf(stderr, "Process %1d failed to multicast STARTED message: %s\n", s->id, strerror(errno));
        fflush(s->events_log);
        return 1;
    }
    fwrite(msg.s_payload, sizeof(char), msg.s_header.s_payload_len, s->events_log);
    fwrite(msg.s_payload, sizeof(char), msg.s_header.s_payload_len, stdout);
    fflush(s->events_log);

    while (started != s->nbr_count - 1) {
        if (receive_any(s, &msg) != 0) {
            fprintf(s->events_log, "Process %1d failed to receive message: %s\n", s->id, strerror(errno));
            fprintf(stderr, "Process %1d failed to receive message: %s\n", s->id, strerror(errno));
            fflush(s->events_log);
            return 1;
        }
        if (msg.s_header.s_type == STARTED) started++;
        if (msg.s_header.s_type == DONE) done++;
    }
    fprintf(s->events_log, log_received_all_started_fmt, s->id);
    fprintf(stdout, log_received_all_started_fmt, s->id);
    fflush(s->events_log);

    msg = (Message) { .s_header = { .s_magic = MESSAGE_MAGIC, .s_type = DONE, .s_local_time = s->id } };
    sprintf(msg.s_payload, log_done_fmt, s->id);
    msg.s_header.s_payload_len = strlen(msg.s_payload);
    if (send_multicast(s, &msg) != 0) {
        fprintf(s->events_log, "Process %1d failed to multicast DONE message: %s\n", s->id, strerror(errno));
        fprintf(stderr, "Process %1d failed to multicast DONE message: %s\n", s->id, strerror(errno));
        fflush(s->events_log);
        return 1;
    }
    fwrite(msg.s_payload, sizeof(char), msg.s_header.s_payload_len, s->events_log);
    fwrite(msg.s_payload, sizeof(char), msg.s_header.s_payload_len, stdout);
    fflush(s->events_log);

    while (done != s->nbr_count - 1) {
        if (receive_any(s, &msg) != 0) {
            fprintf(s->events_log, "Process %1d failed to receive message: %s\n", s->id, strerror(errno));
            fprintf(stderr, "Process %1d failed to receive message: %s\n", s->id, strerror(errno));
            fflush(s->events_log);
            return 1;
        }
        if (msg.s_header.s_type == DONE) done++;
    }
    fprintf(s->events_log, log_received_all_done_fmt, s->id);
    fprintf(stdout, log_received_all_done_fmt, s->id);
    fflush(s->events_log);

    return 0;
}

int run_parent_worker(Worker* s) {
    Message msg;
    size_t started = 0;
    size_t done = 0;

    while (started != s->nbr_count) {
        if (receive_any(s, &msg) != 0) {
            fprintf(s->events_log, "Process %1d failed to receive message: %s\n", s->id, strerror(errno));
            fprintf(stderr, "Process %1d failed to receive message: %s\n", s->id, strerror(errno));
            fflush(s->events_log);
            return 1;
        }
        if (msg.s_header.s_type == STARTED) started++;
        if (msg.s_header.s_type == DONE) done++;
    }
    fprintf(s->events_log, log_received_all_started_fmt, s->id);
    fprintf(stdout, log_received_all_started_fmt, s->id);
    fflush(s->events_log);

    while (done != s->nbr_count) {
        if (receive_any(s, &msg) != 0) {
            fprintf(s->events_log, "Process %1d failed to receive message: %s\n", s->id, strerror(errno));
            fprintf(stderr, "Process %1d failed to receive message: %s\n", s->id, strerror(errno));
            fflush(s->events_log);
            return 1;
        }
        if (msg.s_header.s_type == DONE) done++;
    }
    fprintf(s->events_log, log_received_all_done_fmt, s->id);
    fprintf(stdout, log_received_all_done_fmt, s->id);
    fflush(s->events_log);

    return 0;
}

int arg_parse(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s -p X\n", argv[0]);
        return -1;
    }
    if (strcmp(argv[1], "-p")) {
        fprintf(stderr, "usage: %s -p X\n", argv[0]);
        return -1;
    }
    int x = atoi(argv[2]);
    if (x <= 0) {
        fprintf(stderr, "error: Number of processes must be a positive integer\n");
        return -1;
    }
    return x;
}

int main(int argc, char** argv) {
    int result = 0;
    Worker* workers = NULL;
    Worker* s;

    int workers_count = arg_parse(argc, argv);
    if (workers_count <= 0) return 1;

    FILE* pipes_log_fd = fopen(pipes_log, "a");
    if (pipes_log_fd == NULL) {
        fprintf(stderr, "Failed to open file %s: %s", pipes_log, strerror(errno));
        return 1;
    }

    FILE* events_log_fd = fopen(events_log, "a");
    if (events_log_fd == NULL) {
        fprintf(stderr, "Failed to open file %s: %s", events_log, strerror(errno));
        return 1;
    }

    workers = calloc(workers_count + 1, sizeof(Worker));
    if (init_workers(workers, workers_count, events_log_fd, pipes_log_fd) != 0) defer_return(1);
    fflush(pipes_log_fd); // flush to avoid writing the same buffer again from workers

    for (worker_id worker_id = PARENT_ID + 1; worker_id < workers_count + 1; worker_id++) {
        int worker_pid = fork();
        switch (worker_pid) {
        case -1: {
            fprintf(stderr, "Failed to fork a new process for worker %d: %s\n", worker_id, strerror(errno));
            defer_return(2);
        } break;
        case 0: {
            s = &workers[worker_id];
            deinit_unused_channels(s, workers, pipes_log_fd);
            int status = run_worker(s);
            defer_return(status);
        } break;
        }
    }

    s = &workers[PARENT_ID];
    deinit_unused_channels(s, workers, pipes_log_fd);
    int status = run_parent_worker(s);
    while (wait(NULL) > 0);
    defer_return(status);

defer:
    if (workers != NULL) deinit_workers(s, workers, pipes_log_fd);
    fclose(pipes_log_fd);
    fclose(events_log_fd);
    return result;
}
