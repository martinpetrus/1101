#include <semaphore.h>
#include "event.h"

sem_t sem_trigger_event, sem_action_event;
extern sem_t *trigger_update_sems, *trigger_updated_sems;
extern sem_t *action_update_sems, *action_updated_sems;
extern int trigger_count;
extern int action_count;

void notify_triggers() {
    int i;
    sem_wait(&sem_trigger_event);
    for (i = 0; i < trigger_count; i++) {
        sem_post(&trigger_update_sems[i]);
    }
    for (i = 0; i < trigger_count; i++) {
        sem_wait(&trigger_updated_sems[i]);
    }
    sem_post(&sem_trigger_event);
}

void notify_actions() {
    int i;
    sem_wait(&sem_action_event);
    for (i = 0; i < action_count; i++) {
        sem_post(&action_update_sems[i]);
    }
    for (i = 0; i < action_count; i++) {
        sem_wait(&action_updated_sems[i]);
    }
    sem_post(&sem_action_event);
}

void init_event_publisher() {
    sem_init(&sem_trigger_event, 0, 1);
    sem_init(&sem_action_event, 0, 1);
}