#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "timer.h"
#include "event.h"

void *thread_timer(void * arg) {

    extern int timer_time, ext_sync, signature, RND, shutdown;
    extern unsigned int BPM;

    int i = 0;

    while (!shutdown) {
        timer_time = i++;
        i %= signature;
        notify_actions();
        usleep(LENGTH_OF_A_TICK_AT_ONE_BPM / BPM);

        RND = rand() > ((RAND_MAX) / 2) - 1 ? 1 : 0;
    }
    return NULL;
}