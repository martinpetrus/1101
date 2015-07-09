#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <regex.h>

const struct timespec time_out = {READ_TIMEOUT, 0};

void start_thread(pthread_t *thread, void *(*__start_routine)(void *), void *arg, const char *error_message) {
    int res = pthread_create(thread, NULL, __start_routine, arg);
    handle_result(res, error_message);
}

void start_realtime_thread(pthread_t *thread, void *(*__start_routine)(void *), const char *error_message) {
    pthread_attr_t attr;

    handle_result(pthread_attr_init(&attr), "Error initializing thread attributes\n");
    handle_result(pthread_attr_setschedpolicy(&attr, SCHED_RR), "Error applying scheduling policy\n");
    handle_result(pthread_create(thread, &attr, __start_routine, NULL), error_message);
    handle_result(pthread_attr_destroy(&attr), "Error destroying thread attributes\n");
}

void exit_thread(const char *type, int index, const char *name) {
    char *ret_message = smalloc(100 * sizeof(char));
    sprintf(ret_message, "Exitting %s thread %d (%s)\n", type, index, name);
    pthread_exit((void *) ret_message);
}

int regex_match(const char *input, const char *pattern) {
    regex_t regex;
    int ret;

    if (regcomp(&regex, pattern, 0)) {
        perror("Error compiling pattern\n");
        exit(EXIT_FAILURE);
    }

    ret = regexec(&regex, input, 0, NULL, 0);
    switch (ret) {
        case REG_NOERROR:
            return 1;
        case REG_NOMATCH:
            return 0;
        default:
            perror("Error executing regex\n");
            exit(EXIT_FAILURE);
    }

}

int wait_for_input(int fd) {
    fd_set *set = smalloc(sizeof(fd_set));
    FD_SET(fd, set);

    int ret = pselect(fd + 1, set, NULL, NULL, &time_out, NULL);
    free(set);
    if (ret < 0) {
        perror("An error occuted while waiting for input\n");
        exit(EXIT_FAILURE);
    }
    return ret;
};

int wait_for_sem(sem_t *sem) {
    struct timespec abs_time_out;
    if (clock_gettime(CLOCK_REALTIME, &abs_time_out) == -1) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }
    abs_time_out.tv_sec += READ_TIMEOUT;
    return sem_timedwait(sem, &abs_time_out);
};

unsigned char get_tone(unsigned short input, scale_t scale) {

    unsigned char i, j = 0;

    unsigned char base = (unsigned char) ((input >> 9) - (input >> 9) % 12);
    int note_index = scale.tones * (input % (12 << 9)) / (12 << 9);

    for (i = 0; i < 12; i++) {
        if (scale.mask & 0x800U >> i) {
            if (j++ == note_index) {
                return base + i;
            }
        }
    }
}

char *read_file(const char *file) {
    FILE *fp;
    unsigned long size;
    char *buffer;

    fp = fopen(file, "rb");
    if (fp == NULL) {
        perror("error reading file");
        exit(EXIT_FAILURE);
    }

    fseek(fp, 0L, SEEK_END);
    size = (unsigned long) ftell(fp);
    rewind(fp);

    buffer = smalloc(sizeof(char) * (size + 1));

    if (fread(buffer, size, 1, fp) != 1) {
        perror("Reading file failed");
        exit(EXIT_FAILURE);
    }

    fclose(fp);

    return buffer;
}