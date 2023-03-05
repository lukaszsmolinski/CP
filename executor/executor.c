#include "err.h"
#include "utils.h"
#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_LEN_STATUS 128
#define MAX_LEN_CMD 512
#define MAX_LEN_OUT 1024
#define MAX_N_TASKS 4096

struct Task {
    char output[2][MAX_LEN_OUT];
    char error[2][MAX_LEN_OUT];
    atomic_int last_output;
    atomic_int last_error;
    pid_t pid;
    int id;
};

struct Sync {
    sem_t outer_command_mutex;
    sem_t command_mutex;
    sem_t end_mutex;
    sem_t end_queue;
    sem_t waiter;
    char end_status[MAX_LEN_STATUS];
    atomic_bool quit;
    atomic_int active_tasks;
    atomic_int waiting_end;
};

static void read_stream(int fd, char buffer[2][MAX_LEN_OUT], atomic_int* current_buffer) {
    FILE* file = fdopen(fd, "r");
    while (file != NULL && read_line(buffer[1^*current_buffer], MAX_LEN_OUT, file)) {
        buffer[1^*current_buffer][strcspn(buffer[1^*current_buffer], "\n")] = '\0';
        *current_buffer ^= 1;
    }
    ASSERT_ZERO(fclose(file));
}

static void print_end_status(struct Sync* sync, int id, int status) {
    ASSERT_SYS_OK(sem_wait(&sync->end_mutex));
    if (++sync->waiting_end == 1) {
        ASSERT_SYS_OK(sem_post(&sync->end_mutex));
        ASSERT_SYS_OK(sem_wait(&sync->outer_command_mutex));
        ASSERT_SYS_OK(sem_wait(&sync->command_mutex));
        ASSERT_SYS_OK(sem_post(&sync->outer_command_mutex));
    } else {
        ASSERT_SYS_OK(sem_post(&sync->end_mutex));
        ASSERT_SYS_OK(sem_wait(&sync->end_queue));
    }
    ASSERT_SYS_OK(sem_wait(&sync->end_mutex));

    int ret = WIFEXITED(status)
        ? snprintf(sync->end_status, MAX_LEN_STATUS,
                    "Task %d ended: status %d.\n", id, WEXITSTATUS(status))
        : snprintf(sync->end_status, MAX_LEN_STATUS,
                    "Task %d ended: signalled.\n", id);
    if (ret < 0 || ret >= MAX_LEN_STATUS)
        fatal("snprintf");

    ASSERT_SYS_OK(sem_post(&sync->waiter));
}

static void* waiter(void* data) {
    struct Sync* sync = (struct Sync*)data;
    bool quit = false;
    while (!quit) {
        ASSERT_SYS_OK(sem_wait(&sync->waiter));
        if (sync->active_tasks == 0 && sync->quit)
            break;

        ASSERT_SYS_OK(wait(NULL));
        --sync->active_tasks;
        quit = sync->active_tasks == 0 && sync->quit;
        printf("%s", sync->end_status);
        fflush(stdout);

        if (--sync->waiting_end > 0)
            ASSERT_SYS_OK(sem_post(&sync->end_queue));
        else
            ASSERT_SYS_OK(sem_post(&sync->command_mutex));
        ASSERT_SYS_OK(sem_post(&sync->end_mutex));
    }
    pthread_exit(NULL);
}

static void run_and_exit(char** cmd, struct Task* task, struct Sync* sync) {
    int pipe_out[2], pipe_err[2];
    ASSERT_SYS_OK(pipe(pipe_out));
    ASSERT_SYS_OK(pipe(pipe_err));

    pid_t task_pid = fork();
    ASSERT_SYS_OK(task_pid);
    if (task_pid == 0) {
        task->pid = getpid();
        ++sync->active_tasks;
        printf("Task %d started: pid %d.\n", task->id, task->pid);
        fflush(stdout);
        ASSERT_SYS_OK(sem_post(&sync->command_mutex));

        ASSERT_SYS_OK(close(pipe_out[0]));
        ASSERT_SYS_OK(close(pipe_err[0]));
        ASSERT_SYS_OK(dup2(pipe_out[1], STDOUT_FILENO));
        ASSERT_SYS_OK(close(pipe_out[1]));
        ASSERT_SYS_OK(dup2(pipe_err[1], STDERR_FILENO));
        ASSERT_SYS_OK(close(pipe_err[1]));

        ASSERT_SYS_OK(execvp(*cmd, cmd));
    } else {
        ASSERT_SYS_OK(close(pipe_out[1]));
        ASSERT_SYS_OK(close(pipe_err[1]));

        pid_t pid = fork();
        ASSERT_SYS_OK(pid);
        if (pid == 0) {
            ASSERT_SYS_OK(close(pipe_out[0]));
            read_stream(pipe_err[0], task->error, &task->last_error);
        } else {
            ASSERT_SYS_OK(close(pipe_err[0]));
            read_stream(pipe_out[0], task->output, &task->last_output);

            int status;
            ASSERT_SYS_OK(waitpid(pid, NULL, 0));
            ASSERT_SYS_OK(waitpid(task_pid, &status, 0));
            print_end_status(sync, task->id, status);
        }
    }
    _exit(0);
}

static void parse_command(struct Task* tasks, struct Sync* sync, char** cmd, int* task_id) {
    ASSERT_SYS_OK(sem_wait(&sync->outer_command_mutex));
    ASSERT_SYS_OK(sem_wait(&sync->command_mutex));
    ASSERT_SYS_OK(sem_post(&sync->outer_command_mutex));
    if (strcmp(*cmd, "run") == 0) {
        tasks[*task_id].id = *task_id;
        pid_t pid = fork();
        ASSERT_SYS_OK(pid);
        if (pid == 0)
            run_and_exit(&cmd[1], &tasks[*task_id], sync);
        else
            ++*task_id;
    } else {
        if (strcmp(*cmd, "out") == 0) {
            int t = atoi(cmd[1]);
            char* out = tasks[t].output[tasks[t].last_output];
            printf("Task %d stdout: '%s'.\n", t, out);
            fflush(stdout);
        } else if (strcmp(*cmd, "err") == 0) {
            int t = atoi(cmd[1]);
            char* err = tasks[t].error[tasks[t].last_error];
            printf("Task %d stderr: '%s'.\n", t, err);
            fflush(stdout);
        } else if (strcmp(*cmd, "kill") == 0) {
            int t = atoi(cmd[1]);
            if (tasks[t].pid != 0)
                kill(tasks[t].pid, SIGINT);
        } else if (strcmp(*cmd, "sleep") == 0) {
            int n = atoi(cmd[1]);
            ASSERT_SYS_OK(usleep(1000 * n));
        } else if (strcmp(*cmd, "quit") == 0) {
            sync->quit = true;
            if (sync->active_tasks == 0)
                ASSERT_SYS_OK(sem_post(&sync->waiter));
        }
        ASSERT_SYS_OK(sem_post(&sync->command_mutex));
    }
}

int main(void) {
    struct Task* tasks = (struct Task*)mmap(NULL, MAX_N_TASKS * sizeof(struct Task),
        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    struct Sync* sync = (struct Sync*)mmap(NULL, sizeof(struct Sync),
        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (tasks == MAP_FAILED || sync == MAP_FAILED)
        syserr("mmap");
    ASSERT_SYS_OK(sem_init(&sync->outer_command_mutex, 1, 1));
    ASSERT_SYS_OK(sem_init(&sync->command_mutex, 1, 1));
    ASSERT_SYS_OK(sem_init(&sync->end_mutex, 1, 1));
    ASSERT_SYS_OK(sem_init(&sync->end_queue, 1, 0));
    ASSERT_SYS_OK(sem_init(&sync->waiter, 1, 0));

    pthread_t waiter_thread;
    ASSERT_ZERO(pthread_create(&waiter_thread, NULL, waiter, (void*)sync));

    int task_id = 0;
    char buffer[MAX_LEN_CMD];
    while (!sync->quit && read_line(buffer, MAX_LEN_CMD, stdin)) {
        buffer[strcspn(buffer, "\n")] = '\0';
        char** cmd = split_string(buffer);
        parse_command(tasks, sync, cmd, &task_id);
        free_split_string(cmd);
    }

    ASSERT_SYS_OK(sem_wait(&sync->outer_command_mutex));
    ASSERT_SYS_OK(sem_wait(&sync->command_mutex));
    ASSERT_SYS_OK(sem_post(&sync->outer_command_mutex));
    if (!sync->quit) {
        sync->quit = true;
        if (sync->active_tasks == 0)
            ASSERT_SYS_OK(sem_post(&sync->waiter));
    }
    ASSERT_SYS_OK(sem_post(&sync->command_mutex));

    for (int i = 0; i < MAX_N_TASKS && tasks[i].pid != 0; ++i)
        kill(tasks[i].pid, SIGKILL);
    ASSERT_ZERO(pthread_join(waiter_thread, NULL));

    ASSERT_SYS_OK(sem_destroy(&sync->outer_command_mutex));
    ASSERT_SYS_OK(sem_destroy(&sync->command_mutex));
    ASSERT_SYS_OK(sem_destroy(&sync->end_mutex));
    ASSERT_SYS_OK(sem_destroy(&sync->end_queue));
    ASSERT_SYS_OK(sem_destroy(&sync->waiter));
    ASSERT_SYS_OK(munmap(tasks, MAX_N_TASKS * sizeof(struct Task)));
    ASSERT_SYS_OK(munmap(sync, sizeof(struct Sync)));

    return 0;
}
