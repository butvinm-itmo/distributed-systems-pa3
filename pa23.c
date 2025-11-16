
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
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
#include "lamport.h"
#include "pa2345.h"
#include "worker.h"

#define defer_return(r) \
    do {                \
        result = r;     \
        goto defer;     \
    } while (0)

void log_event(FILE* events_log, FILE* out, const char* fmt, ...) {
    va_list args1, args2;

    va_start(args1, fmt);
    va_copy(args2, args1);

    vfprintf(events_log, fmt, args1);
    vfprintf(out, fmt, args2);
    fflush(events_log);

    va_end(args1);
    va_end(args2);
}

typedef struct {
    Worker* worker;
    AllHistory history;
} BankClientWorker;

typedef struct {
    timestamp_t sent_at;
    timestamp_t received_at;
    balance_t amount;
} ReceivedTransfer;

typedef struct {
    Worker* worker;
    balance_t balance;
    BalanceHistory history;
    ReceivedTransfer received_transfers[MAX_T];
    int received_count;
} BankAccountWorker;

static balance_t calculate_pending_at(ReceivedTransfer* transfers, int count, timestamp_t t) {
    balance_t pending = 0;
    for (int i = 0; i < count; i++) {
        if (transfers[i].sent_at <= t && t < transfers[i].received_at) {
            pending += transfers[i].amount;
        }
    }
    return pending;
}

static void update_balance_history(BankAccountWorker* s, timestamp_t from_time, timestamp_t to_time, balance_t current_balance) {
    balance_t base_balance = (s->history.s_history_len > 0) ? s->history.s_history[s->history.s_history_len - 1].s_balance : current_balance;

    for (timestamp_t t = from_time; t <= to_time; t++) {
        s->history.s_history[t].s_time = t;
        s->history.s_history[t].s_balance = (t < to_time) ? base_balance : current_balance;
        s->history.s_history[t].s_balance_pending_in = calculate_pending_at(s->received_transfers, s->received_count, t);
    }
    s->history.s_history_len = to_time + 1;
}

int execute_bank_account_worker(BankAccountWorker s) {
    timestamp_t timestamp;
    Message msg;
    size_t started = 0;
    size_t done = 0;

    increment_lamport_time();
    timestamp = get_lamport_time();
    msg = (Message) { .s_header = { .s_magic = MESSAGE_MAGIC, .s_type = STARTED, .s_local_time = timestamp } };
    sprintf(msg.s_payload, log_started_fmt, timestamp, s.worker->id, getpid(), getppid(), s.balance);
    msg.s_header.s_payload_len = strlen(msg.s_payload);
    if (send_multicast(s.worker, &msg) != 0) {
        log_event(s.worker->events_log, stderr, "Process %1d failed to multicast STARTED message: %s\n", s.worker->id, strerror(errno));
        return 1;
    }
    log_event(s.worker->events_log, stdout, log_started_fmt, timestamp, s.worker->id, getpid(), getppid(), s.balance);

    while (started != s.worker->nbr_count - 1 || done != s.worker->nbr_count - 1) {
        if (receive_any(s.worker, &msg) != 0) {
            log_event(s.worker->events_log, stderr, "Process %1d failed to receive message: %s\n", s.worker->id, strerror(errno));
            return 1;
        }
        update_lamport_time(msg.s_header.s_local_time);
        timestamp = get_lamport_time();

        switch (msg.s_header.s_type) {
        case (STARTED): {
            started++;
            if (started == s.worker->nbr_count - 1) {
                log_event(s.worker->events_log, stdout, log_received_all_started_fmt, timestamp, s.worker->id);
            }
        } break;
        case (TRANSFER): {
            TransferOrder order = *(TransferOrder*)msg.s_payload;

            if (order.s_src == s.worker->id) {
                increment_lamport_time();
                timestamp_t send_time = get_lamport_time();

                s.balance -= order.s_amount;

                update_balance_history(&s, s.history.s_history_len, send_time, s.balance);

                msg = (Message) { .s_header = { .s_magic = MESSAGE_MAGIC, .s_type = TRANSFER, .s_local_time = send_time, .s_payload_len = sizeof(TransferOrder) } };
                memcpy(msg.s_payload, &order, sizeof(TransferOrder));
                if (send(s.worker, order.s_dst, &msg) != 0) {
                    log_event(s.worker->events_log, stderr, "Process %1d failed to send TRANSFER message to %1d: %s\n", s.worker->id, order.s_dst, strerror(errno));
                    return 1;
                }
                log_event(s.worker->events_log, stdout, log_transfer_out_fmt, send_time, s.worker->id, order.s_amount, order.s_dst);
            } else {
                timestamp_t transfer_sent_time = msg.s_header.s_local_time;

                s.received_transfers[s.received_count].sent_at = transfer_sent_time;
                s.received_transfers[s.received_count].received_at = timestamp;
                s.received_transfers[s.received_count].amount = order.s_amount;
                s.received_count++;

                s.balance += order.s_amount;

                update_balance_history(&s, s.history.s_history_len, timestamp, s.balance);

                increment_lamport_time();
                timestamp_t ack_time = get_lamport_time();
                msg = (Message) { .s_header = { .s_magic = MESSAGE_MAGIC, .s_type = ACK, .s_local_time = ack_time } };
                if (send(s.worker, PARENT_ID, &msg) != 0) {
                    log_event(s.worker->events_log, stderr, "Process %1d failed to send TRANSFER message to %1d: %s\n", s.worker->id, PARENT_ID, strerror(errno));
                    return 1;
                }
                log_event(s.worker->events_log, stdout, log_transfer_in_fmt, timestamp, s.worker->id, order.s_amount, order.s_src);
            }
        } break;
        case (STOP): {
            increment_lamport_time();
            timestamp_t done_time = get_lamport_time();
            msg = (Message) { .s_header = { .s_magic = MESSAGE_MAGIC, .s_type = DONE, .s_local_time = done_time } };
            sprintf(msg.s_payload, log_done_fmt, done_time, s.worker->id, s.balance);
            msg.s_header.s_payload_len = strlen(msg.s_payload);
            if (send_multicast(s.worker, &msg) != 0) {
                log_event(s.worker->events_log, stderr, "Process %1d failed to multicast DONE message: %s\n", s.worker->id, strerror(errno));
                return 1;
            }
            log_event(s.worker->events_log, stdout, log_done_fmt, done_time, s.worker->id, s.balance);
        } break;
        case (DONE): {
            done++;
            if (done == s.worker->nbr_count - 1) {
                log_event(s.worker->events_log, stdout, log_received_all_done_fmt, timestamp, s.worker->id);
            }
        } break;
        default: {
            log_event(s.worker->events_log, stderr, "Process %1d received unexpected message [%d]\n", s.worker->id, msg.s_header.s_type);
            return 1;
        } break;
        }
    }

    timestamp = get_lamport_time();
    update_balance_history(&s, s.history.s_history_len, timestamp, s.balance);

    increment_lamport_time();
    timestamp_t history_time = get_lamport_time();
    size_t payload_len = sizeof(s.history.s_id) + sizeof(s.history.s_history_len) + s.history.s_history_len * sizeof(BalanceState);
    msg = (Message) { .s_header = { .s_magic = MESSAGE_MAGIC, .s_type = BALANCE_HISTORY, .s_local_time = history_time, .s_payload_len = payload_len } };
    memcpy(msg.s_payload, &s.history, payload_len);
    if (send(s.worker, PARENT_ID, &msg) != 0) {
        log_event(s.worker->events_log, stderr, "Process %1d failed to send BALANCE_HISTORY message to %1d: %s\n", s.worker->id, PARENT_ID, strerror(errno));
        return 1;
    }
    return 0;
}

int execute_bank_client_worker(BankClientWorker s) {
    timestamp_t timestamp;
    Message msg;
    size_t started = 0;
    size_t done = 0;

    while (started != s.worker->nbr_count || done != s.worker->nbr_count) {
        if (receive_any(s.worker, &msg) != 0) {
            log_event(s.worker->events_log, stderr, "Process %1d failed to receive message: %s\n", s.worker->id, strerror(errno));
            return 1;
        }
        update_lamport_time(msg.s_header.s_local_time);
        timestamp = get_lamport_time();

        switch (msg.s_header.s_type) {
        case (STARTED): {
            started++;
            if (started == s.worker->nbr_count) {
                log_event(s.worker->events_log, stdout, log_received_all_started_fmt, timestamp, s.worker->id);

                bank_robbery(&s, s.worker->nbr_count);

                increment_lamport_time();
                timestamp_t stop_time = get_lamport_time();
                msg = (Message) { .s_header = { .s_magic = MESSAGE_MAGIC, .s_type = STOP, .s_local_time = stop_time } };
                if (send_multicast(s.worker, &msg) != 0) {
                    log_event(s.worker->events_log, stderr, "Process %1d failed to multicast STOP message: %s\n", s.worker->id, strerror(errno));
                    return 1;
                }
            }
        } break;
        case (DONE): {
        } break;
        case (BALANCE_HISTORY): {
            BalanceHistory history = *(BalanceHistory*)msg.s_payload;
            s.history.s_history[history.s_id - 1] = history;
            done++;
            if (done == s.worker->nbr_count) {
                log_event(s.worker->events_log, stdout, log_received_all_done_fmt, timestamp, s.worker->id);
            }
        } break;
        default: {
            log_event(s.worker->events_log, stderr, "Process %1d received unexpected message [%d]\n", s.worker->id, msg.s_header.s_type);
            return 1;
        } break;
        }
    }

    print_history(&s.history);
    return 0;
}

void transfer(void* parent_data, local_id src, local_id dst, balance_t amount) {
    BankClientWorker* s = (BankClientWorker*)parent_data;
    timestamp_t timestamp;
    Message msg;

    TransferOrder order = { .s_src = src, .s_dst = dst, .s_amount = amount };

    increment_lamport_time();
    timestamp = get_lamport_time();
    msg = (Message) { .s_header = { .s_magic = MESSAGE_MAGIC, .s_type = TRANSFER, .s_local_time = timestamp, .s_payload_len = sizeof(order) } };
    memcpy(msg.s_payload, &order, msg.s_header.s_payload_len);
    if (send(s->worker, src, &msg) != 0) {
        log_event(s->worker->events_log, stderr, "Process %1d failed to send TRANSFER message to %1d: %s\n", s->worker->id, src, strerror(errno));
        return;
    }

    if (receive(s->worker, dst, &msg) != 0) {
        log_event(s->worker->events_log, stderr, "Process %1d failed to receive message from %1d: %s\n", s->worker->id, dst, strerror(errno));
        return;
    }
    update_lamport_time(msg.s_header.s_local_time);

    if (msg.s_header.s_type != ACK) {
        log_event(s->worker->events_log, stderr, "Process %1d expected to receive ACK message from %1d, but got [%d]\n", s->worker->id, dst, msg.s_header.s_type);
        return;
    }
}

typedef struct {
    bool ok;
    int bank_account_workers_count; // number of bank account processes
    balance_t initial_balances[MAX_PROCESS_ID + 1]; // initial account balances
} CliArgs;

CliArgs arg_parse(int argc, char** argv) {
    CliArgs args = { .ok = false };

    if (argc < 3) {
        fprintf(stderr, "usage: %s -p X <B1..BX>\n", argv[0]);
        return args;
    }

    if (strcmp(argv[1], "-p") != 0) {
        fprintf(stderr, "usage: %s -p X <B1..BX>\n", argv[0]);
        return args;
    }

    args.bank_account_workers_count = atoi(argv[2]);
    if (args.bank_account_workers_count <= 0) {
        fprintf(stderr, "error: Number of processes must be a positive integer\n");
        return args;
    }

    if (argc != 3 + args.bank_account_workers_count) {
        fprintf(stderr, "error: Process and balances number mismatch\n");
        return args;
    }
    for (int i = PARENT_ID + 1; i <= args.bank_account_workers_count; i++) {
        args.initial_balances[i] = atoi(argv[3 + i - 1]);
        if (args.initial_balances[i] <= 0) {
            fprintf(stderr, "error: Balance must be a positive integer\n");
            return args;
        }
    }

    args.ok = true;
    return args;
}

int main(int argc, char** argv) {
    int result = 0;
    Worker* workers = NULL;
    Worker* w;

    CliArgs args = arg_parse(argc, argv);
    if (!args.ok) return 1;

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

    workers = calloc(args.bank_account_workers_count + 1, sizeof(Worker));
    if (init_workers(workers, args.bank_account_workers_count, events_log_fd, pipes_log_fd) != 0) defer_return(1);
    fflush(pipes_log_fd); // flush to avoid writing the same buffer again from workers

    for (worker_id worker_id = PARENT_ID + 1; worker_id < args.bank_account_workers_count + 1; worker_id++) {
        int worker_pid = fork();
        switch (worker_pid) {
        case -1: {
            fprintf(stderr, "Failed to fork a new process for worker %d: %s\n", worker_id, strerror(errno));
            defer_return(2);
        } break;
        case 0: {
            w = &workers[worker_id];
            BankAccountWorker bank_account_worker = {
                .worker = w,
                .balance = args.initial_balances[worker_id],
                .history = {
                    .s_id = worker_id,
                    .s_history_len = 1,
                    .s_history = { [0] = { .s_time = 0, .s_balance = args.initial_balances[worker_id], .s_balance_pending_in = 0 } },
                },
                .received_count = 0,
            };
            deinit_unused_channels(w, workers, pipes_log_fd);

            int status = execute_bank_account_worker(bank_account_worker);
            defer_return(status);
        } break;
        }
    }

    w = &workers[PARENT_ID];
    BankClientWorker bank_client_worker = { .worker = w, .history = { .s_history_len = args.bank_account_workers_count } };
    deinit_unused_channels(bank_client_worker.worker, workers, pipes_log_fd);

    int status = execute_bank_client_worker(bank_client_worker);
    while (wait(NULL) > 0);
    defer_return(status);

defer:
    if (workers != NULL) deinit_workers(w, workers, pipes_log_fd);
    fclose(pipes_log_fd);
    fclose(events_log_fd);
    return result;
}
