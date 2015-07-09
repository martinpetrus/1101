#ifndef UTILS_HEADER
#define UTILS_HEADER

#include <pthread.h>
#include <semaphore.h>
#include "common.h"

#define READ_TIMEOUT 5

int wait_for_input(int fd);
int wait_for_sem(sem_t *sem);
int regex_match(const char *input, const char *pattern);
void start_thread(pthread_t *thread, void *(*__start_routine)(void *), void *arg, const char *error_message);
void start_realtime_thread(pthread_t *thread, void *(*__start_routine)(void *), const char *error_message);
void exit_thread(const char *type, int index, const char *name);
unsigned char get_tone(unsigned short input, scale_t scale);
char *read_file(const char *file);
#endif