#include <stdlib.h>
#include <stdio.h>
#include <semaphore.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "timer.h"
#include "utils.h"
#include "joystick.h"
#include "midi.h"
#include "trigger.h"
#include "event.h"
#include "action.h"
#include "config.h"


// prototypes
static void start_up();
static void shut_down();
static void sig_handler(int sig);

context_t ctx;

// globals
int shutdown;
int timer_time;
int ext_sync;
unsigned int BPM = 120;
int signature = 96;
int RND;

int scale_count;
scale_t *scales;
int chord_count;
scale_t *chords;

int joystick_count;
js_status_t *joysticks;

int midi_in_count, midi_out_count;
midi_msg_t *midi_in, *midi_out;
sem_t *midi_out_put_sems, *midi_out_take_sems;

int trigger_count;
trigger_t *triggers;
sem_t *trigger_update_sems, *trigger_updated_sems;

int action_count;
action_t *actions;
sem_t *action_update_sems;
sem_t *action_updated_sems;

// threading
pthread_t timer_thread, *joystick_threads, *midi_in_threads, *midi_out_threads, *trigger_threads, *action_threads;


int main() {

    ctx.scale_index = 1;
    ctx.scale_offset = 0;

    start_up();
    while (!shutdown) {
        sleep(1);
    }
    shut_down();
    exit(EXIT_SUCCESS);
}

static void start_up() {

    int i;

    /*initialization*/
    signal(SIGINT, sig_handler);
    signal(SIGSEGV, sig_handler);

    joystick_count = count_joysticks();
    joystick_threads = smalloc(sizeof(pthread_t) * joystick_count);
    joysticks = smalloc(sizeof(js_status_t) + joystick_count);

    init_midi_ports();
    midi_in_threads = smalloc(sizeof(pthread_t) * midi_in_count);
    midi_in = smalloc(sizeof(midi_msg_t) * midi_in_count);
    midi_out_threads = smalloc(sizeof(pthread_t) * midi_out_count);
    midi_out = smalloc(sizeof(midi_msg_t) * midi_out_count);
    midi_out_put_sems = smalloc(sizeof(sem_t) * midi_out_count);
    midi_out_take_sems = smalloc(sizeof(sem_t) * midi_out_count);

    read_config();
    trigger_threads = smalloc(sizeof(pthread_t) * trigger_count);
    trigger_update_sems = smalloc(sizeof(sem_t) * trigger_count);
    trigger_updated_sems = smalloc(sizeof(sem_t) * trigger_count);
    action_threads = smalloc(sizeof(pthread_t) * action_count);
    action_update_sems = smalloc(sizeof(sem_t) * action_count);
    action_updated_sems = smalloc(sizeof(sem_t) * action_count);

    init_event_publisher();

    /*thread initialization*/
    for (i = 0; i < midi_out_count; i++) {
        sem_init(&midi_out_put_sems[i], 0, 1);
        sem_init(&midi_out_take_sems[i], 0, 0);
        int *midi_out_no = smalloc(sizeof(int));
        *midi_out_no = i;
        start_thread(&midi_out_threads[i], thread_midi_out, midi_out_no, "Error starting midi out thread.\n");
    }

    for (i = 0; i < action_count; i++) {
        sem_init(&action_update_sems[i], 0, 0);
        sem_init(&action_updated_sems[i], 0, 0);
        int *action_no = smalloc(sizeof(int));
        *action_no = i;
        start_thread(&action_threads[i], thread_action, action_no, "Error starting action thread.\n");
    }

    for (i = 0; i < trigger_count; i++) {
        sem_init(&trigger_update_sems[i], 0, 0);
        sem_init(&trigger_updated_sems[i], 0, 0);
        int *trigger_no = smalloc(sizeof(int));
        *trigger_no = i;
        start_thread(&trigger_threads[i], thread_trigger, trigger_no, "Error starting trigger thread.\n");
    }

    start_realtime_thread(&timer_thread, thread_timer, "Error starting timer thread.\n");

    for (i = 0; i < joystick_count; i++) {
        int *joy_no = smalloc(sizeof(int));
        *joy_no = i;
        start_thread(&joystick_threads[i], thread_joystick, joy_no, "Error starting joystick thread.\n");
    }

    for (i = 0; i < midi_in_count; i++) {
        int *midi_in_no = smalloc(sizeof(int));
        *midi_in_no = i;
        start_thread(&midi_in_threads[i], thread_midi_in, midi_in_no, "Error starting midi in thread.\n");
    }

}

static void sig_handler(int sig) {
    fprintf(stderr, "\n%s signal received (Error code: %s)\n", strsignal(sig), strerror(errno));
    shutdown = 1;
}

static void shut_down() {
    int i;
    void *ret;

    printf("Shutting down...\n");
    pthread_join(timer_thread, NULL);
    for (i = 0; i < joystick_count; i++) {
        pthread_join(joystick_threads[i], &ret);
#ifdef _DEBUG
        printf((char *)ret);
#endif
    }
    for (i = 0; i < midi_in_count; i++) {
        pthread_join(midi_in_threads[i], &ret);
#ifdef _DEBUG
        printf((char *)ret);
#endif
    }
    for (i = 0; i < midi_out_count; i++) {
        pthread_join(midi_out_threads[i], &ret);
#ifdef _DEBUG
        printf((char *)ret);
#endif
    }
    for (i = 0; i < trigger_count; i++) {
        pthread_join(trigger_threads[i], &ret);
#ifdef _DEBUG
        printf((char *)ret);
#endif
    }
    for (i = 0; i < action_count; i++) {
        pthread_join(action_threads[i], &ret);
#ifdef _DEBUG
        printf((char *)ret);
#endif
    }
    printf("Done\n");
}

