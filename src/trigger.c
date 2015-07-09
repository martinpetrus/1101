#include <semaphore.h>
#include <stdio.h>
#include "trigger.h"
#include "utils.h"
#include "event.h"
#include "common.h"

extern int shutdown;

extern sem_t *trigger_update_sems, *trigger_updated_sems;

extern int joystick_count;
extern js_status_t *joysticks;
extern int midi_in_count;
extern midi_msg_t *midi_in;
extern trigger_t *triggers;



static void update_trigger(int trigger_no);

void *thread_trigger(void *arg) {

    int trigger_no = *(int *) arg;

    while (!shutdown) {
        if (wait_for_sem(&trigger_update_sems[trigger_no]) == -1) {
            continue;
        }
        update_trigger(trigger_no);
        sem_post(&trigger_updated_sems[trigger_no]);

        notify_actions();
    }

    exit_thread("trigger", trigger_no, triggers[trigger_no].name);
}

static void update_trigger(int trigger_no) {

    trigger_t *trigger = &triggers[trigger_no];
    int i, current_state = 1;

    for (i = 0; i < trigger->no_buttons; i++) {
        tr_btn_t *button = &trigger->buttons[i];
        if (button->js_index >= joystick_count || button->btn_index >= joysticks[button->js_index].no_buttons) {
            current_state = 0;
            break;
        }
        int actual_value = joysticks[button->js_index].buttons[button->btn_index];
        if (actual_value != button->btn_value) {
            current_state = 0;
            break;
        }
    }

    if (current_state) {
        for (i = 0; i < trigger->no_midi_in; i++) {
            tr_midi_t *midi_trigger = &trigger->midi_ins[i];
            if (midi_trigger->port_index >= midi_in_count) {
                current_state = 0;
                break;
            }
            midi_msg_t *in_msg = &midi_in[midi_trigger->port_index];
            if (in_msg->status > midi_trigger->status_hi
                    || in_msg->status < midi_trigger->status_lo
                    || in_msg->data[0] > midi_trigger->msb_hi
                    || in_msg->data[0] < midi_trigger->msb_lo
                    || in_msg->data[1] > midi_trigger->lsb_hi
                    || in_msg->data[1] < midi_trigger->lsb_lo) {
                current_state = 0;
                break;
            }
        }
    }

    int original_status = trigger->status;
    if (trigger->type == PRESS_RELEASE) {
        trigger->status = current_state;
    } else /*if (trigger->type == TWO_WAY)*/ {
        if (current_state > trigger->current_state) { // going from Off to On
            trigger->status ^= 1;
        }
    }
    trigger->current_state = current_state;

#ifdef _DEBUG
    if (original_status != trigger->status) {
        printf("Trigger %d (%s) %s (%d)\n", trigger_no, trigger->name, trigger->status == 0 ? "OFF" : "ON", trigger->status);
    }
#endif

}