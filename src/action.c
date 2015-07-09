#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "utils.h"
#include "midi.h"

extern context_t ctx;

extern int shutdown;
extern int timer_time;
extern int signature;
extern int trigger_count;
extern trigger_t *triggers;
extern action_t *actions;
extern sem_t *action_update_sems, *action_updated_sems;
extern sem_t *midi_out_put_sems, *midi_out_take_sems;
extern midi_msg_t *midi_in, *midi_out;

extern js_status_t *joysticks;

static void on_action(action_t *action, off_ac_t *off_data);
static void off_action(action_t *action, off_ac_t *off_data);
static void update_action(action_t *action);
static midi_msg_t resolve_midi_msg(ac_midi_data_t midi_data);
static unsigned char resolve_action_data(ac_data_type type, ac_data_t ac_data);


void *thread_action(void *arg) {
    int action_no = *(int *) arg;

    action_t *action = &actions[action_no];
//    off_ac_t *on_data = smalloc(sizeof(off_ac_t));
    off_ac_t *off_data = smalloc(sizeof(off_ac_t));

    int end_time = -1;

    while (!shutdown) {
        if (wait_for_sem(&action_update_sems[action_no]) == -1) {
            continue;
        }
        int status = action->status;
        update_action(action);
        sem_post(&action_updated_sems[action_no]);

        if (action->status > status) {
            on_action(action, off_data);
            if (action->length) {
                end_time = (timer_time + action->length) % signature;
            }
        }

        if (end_time != -1) {
            if (timer_time == end_time || end_time > signature) {
                off_action(action, off_data);
                if (action->recurring && action->status) {
                    on_action(action, off_data);
                    end_time = (timer_time + action->length) % signature;
                } else {
                    end_time = -1;
                }
            }
        } else if (action->status < status) {
            off_action(action, off_data);
        }
    }

    exit_thread("action", action_no, action->name);

}

static void update_action(action_t *action) {
    int i, status = 1;
    if (!action->status && action->quantize
            && (timer_time % action->quantize || (signature - timer_time) >= action->length)) {
        return;
    }
    for (i = 0; i < action->no_triggers; i++) {
        if (action->trigger_indices[i] > trigger_count
                || !triggers[action->trigger_indices[i]].status) {
            status = 0;
            break;
        }
    }
    action->status = status;
}

static void off_action(action_t *action, off_ac_t *off_data) {
    int i, port_no;
    switch (off_data->type) {
        case MIDI_OUT:
            port_no = action->on_data.midi_data.port;
            send_message(off_data->data.midi_data, port_no);
            break;
        case OTHER:
        default:
            printf("Action '%s' OFF\n", action->name);
    }
}

static void on_action(action_t *action, off_ac_t *off_data) {
    int i, j, port_no;

    switch (action->type) {
        case MIDI_OUT:
            port_no = action->on_data.midi_data.port;

            // TODO sync access to the js struct for this block
            midi_msg_t msg = resolve_midi_msg(action->on_data.midi_data);
            if (&action->off_data != NULL) {
                off_data->type = MIDI_OUT;
                off_data->data.midi_data = resolve_midi_msg(action->off_data.midi_data);
            }

            send_message(msg, port_no);

            // send additional notes in case of a chord
            if (action->on_data.midi_data.data_type[1] == JS_AXIS) {
                unsigned char base_tone = msg.data[1];
                scale_t chord = ctx.chords[action->on_data.midi_data.data->js_data.chord_index];
                for (i = 1; i < chord.tones; i++) {
                    unsigned char tone = base_tone + chord.tone_positions[i];
                    msg.data[1] = tone > 127 ? tone - (unsigned char) 12 : tone;
                    send_message(msg, port_no);
                }
            }

            printf("(%d) Action '%s' ON - Midi out on port %d\n", timer_time, action->name, port_no);
            break;
        case OTHER:
        default:
            printf("Action '%s' ON\n", action->name);
    }
}

static midi_msg_t resolve_midi_msg(ac_midi_data_t midi_data) {
    unsigned char status = resolve_action_data(midi_data.data_type[0], midi_data.data[0]);
    unsigned char msb = resolve_action_data(midi_data.data_type[1], midi_data.data[1]);
    unsigned char lsb = resolve_action_data(midi_data.data_type[2], midi_data.data[2]);
    return (midi_msg_t) {status, {msb, lsb}, 0};
}

static unsigned char resolve_action_data(ac_data_type type, ac_data_t ac_data) {
    unsigned char ret;
    unsigned short axis;
    switch (type) {
        case JS_AXIS:
            axis = joysticks[ac_data.js_data.js_index].axes[ac_data.js_data.axis_index];
            if (ac_data.js_data.inverted) {
                axis = ~axis;
            }
            switch (ac_data.js_data.filter) {
                case PITCH_FILTER: {
                    scale_t scale = ctx.scale_chord_offset_map[ctx.scale_index][ac_data.js_data.chord_index][ctx.scale_offset];
                    if (scale.mask == 0) {
                        printf("%s contains no tones, playing the respective tone only...\n", scale.name);
                        scale = ctx.scale_chord_offset_map[ctx.scale_index][0][ctx.scale_offset];
                    }
                    return get_tone(axis, scale);
                }
                case MSB_FILTER:
                    return GET_MSB(axis);
                case LSB_FILTER:
                    return GET_LSB(axis);
            }
        case MIDI_IN:
            switch (ac_data.midi_in_data.byte_type) {
                case STATUS:
                    return midi_in[ac_data.midi_in_data.port_index].status;
                case MSB:
                    ret = midi_in[ac_data.midi_in_data.port_index].data[0];
                    return (unsigned char) (ac_data.js_data.inverted ? abs(ret - 127) : ret);
                case LSB:
                    ret = midi_in[ac_data.midi_in_data.port_index].data[1];
                    return (unsigned char) (ac_data.js_data.inverted ? abs(ret - 127) : ret);
            }
        case INTEGER:
            return (unsigned char) (ac_data.int_data & 0xFF);
    }
}




