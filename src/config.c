#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../lib/json.h"
#include "utils.h"
#include "common.h"

#define CONFIG_FILE "config/import.json"
#define NO_SCALE (scale_t) {"No scale", "111111111111", 0xFFF, 12}
#define NO_CHORD (scale_t) {"No chord", "100000000000", 0x800, 1}

static void resolve_action_data(json *json_data, action_data_t **data, action_type type);

extern context_t ctx;

extern int trigger_count;
extern trigger_t *triggers;

extern int action_count;
extern action_t *actions;

static unsigned short l_rotate_scale_mask(unsigned short mask, int shift) {
    return  (unsigned short) (mask << shift & 0xFFF) | (mask >> (12 - shift));
}

static unsigned short filter_scale_for_chord(scale_t scale, scale_t chord, int offset) {

    int i;
    unsigned short mask = 0;

    for (i = 0; i < 12; i++) {
        if ((l_rotate_scale_mask(scale.mask, i) & chord.mask) == chord.mask) {
            mask |= 1 << 11 - i;
        }
    }

    return l_rotate_scale_mask(mask, offset);
}

static unsigned char *get_tone_positions(unsigned short mask, int tone_count) {
    unsigned char i;
    int j;
    unsigned char *tone_positions = smalloc(sizeof(int) * tone_count);

    for (i = 0, j = 0; i < 12 && j < tone_count; i++) {
        if (mask & 0x800U >> i) {
            tone_positions[j] = i;
            j++;
        }
    }
    return tone_positions;
}

static void resolve_scale_types(json *config, scale_t **scale_type, int *counter, const char *types_name, const char *type_name) {

    json *scales_parent = json_get_item(config, types_name);
    if (scales_parent == NULL) {
        return;
    }
    json *scale = scales_parent->child;
    while (scale) {
        *counter += 1;
        *scale_type = srealloc(*scale_type, sizeof(scale_t) * *counter);
        const char *name = strdup(json_get_string(scale, "name", "unknown"));
        const char *value = strdup(json_get_string(scale, "value", "unknown"));
        unsigned short mask = (unsigned short) json_get_int(scale, "mask", 0);
        int tone_count = POPCNT(mask);
        (*scale_type)[*counter - 1] = (scale_t) {name, value, mask, tone_count, get_tone_positions(mask, tone_count)};
#ifdef _DEBUG
        printf("added new %s %s\n", type_name, name);
#endif
        scale = scale->next;
    }
}

static void configure_scales_and_chords(json *config) {
    int i, j, k;
    char *scale_name;
    int scale_count = 1, chord_count = 1;
    scale_t *scales = smalloc(sizeof(scale_t));
    *scales = NO_SCALE;
    scale_t *chords = smalloc(sizeof(scale_t));
    *chords = NO_CHORD;

    resolve_scale_types(config, &scales, &scale_count, "scales", "scale");
    resolve_scale_types(config, &chords, &chord_count, "chords", "chord");

    ctx.chords = chords;
    ctx.scales = scales;

    ctx.scale_chord_offset_map = smalloc(sizeof(scale_t **) * scale_count);
    for (i = 0; i < scale_count; i++) {
        ctx.scale_chord_offset_map[i] = smalloc(sizeof(scale_t *) * chord_count);
        for (j = 0; j < chord_count; j++) {
            ctx.scale_chord_offset_map[i][j] = smalloc(sizeof(scale_t) * 12);
            for (k = 0; k < 12; k++) {
                unsigned short mask = filter_scale_for_chord(scales[i], chords[j], k);
                scale_name = smalloc(sizeof(char) * 100);
                sprintf(scale_name, "'%s' scale filetered for '%s' chord", scales[i].name, chords[j].name);
#ifdef _DEBUG
                printf("%s with offset %d is '%d'\n", scale_name, k, mask);
#endif
                int tone_count = POPCNT(mask);
                ctx.scale_chord_offset_map[i][j][k] = (scale_t) {scale_name, "", mask, tone_count, get_tone_positions(mask, tone_count)};
            }
        }
    }

}

static void configure_triggers(json *config) {

    json *triggers_parent = json_get_item(config, "triggers");
    if (triggers_parent == NULL) {
        perror("Error: no triggers are configured\n");
        exit(EXIT_FAILURE);
    }
    json *trigger = triggers_parent->child;

    while (trigger) {
        trigger_count++;
        triggers = srealloc(triggers, sizeof(trigger_t) * trigger_count);
        const char *tr_name = strdup(json_get_string(trigger, "name", "unknown"));
        const char *tr_type = json_get_string(trigger, "type", "unknown");

        trigger_type type;
        if (strcmp(tr_type, "PRESS_RELEASE") == 0) {
            type = PRESS_RELEASE;
        } else if (strcmp(tr_type, "TWO_WAY") == 0) {
            type = TWO_WAY;
        } else {
            perror("Error: unknown trigger type");
            exit(EXIT_FAILURE);
        }

        int no_buttons = 0;
        tr_btn_t *buttons = NULL;
        json *button_parent = json_get_item(trigger, "buttons");
        if (button_parent != NULL) {
            json *button = button_parent->child;
            while (button) {
                no_buttons++;
                buttons = srealloc(buttons, sizeof(tr_btn_t) * no_buttons);
                int js_index = json_get_int(button, "js_index", -1);
                int btn_index = json_get_int(button, "btn_index", -1);
                int value = json_get_int(button, "value", -1);
                buttons[no_buttons - 1] = (tr_btn_t) {js_index, btn_index, value};
                button = button->next;
            }
        }

        int no_midi_in = 0;
        tr_midi_t *midi_ins = NULL;
        json *midi_in_parent = json_get_item(trigger, "midi_in");
        if (midi_in_parent != NULL) {
            json *midi_in = midi_in_parent->child;
            while (midi_in) {
                no_midi_in++;
                midi_ins = srealloc(midi_ins, sizeof(tr_midi_t) * no_midi_in);
                int port_index = json_get_int(midi_in, "port_index", 0);
                unsigned char status_lo = (unsigned char) json_get_int(midi_in, "status_lo", 0);
                unsigned char status_hi = (unsigned char) json_get_int(midi_in, "status_hi", 0);
                unsigned char msb_lo = (unsigned char) json_get_int(midi_in, "msb_lo", 0);
                unsigned char msb_hi = (unsigned char) json_get_int(midi_in, "msb_hi", 0);
                unsigned char lsb_lo = (unsigned char) json_get_int(midi_in, "lsb_lo", 0);
                unsigned char lsb_hi = (unsigned char) json_get_int(midi_in, "lsb_hi", 0);
                midi_ins[no_midi_in - 1] = (tr_midi_t) {port_index, status_lo, status_hi, msb_lo, msb_hi, lsb_lo, lsb_hi};
                midi_in = midi_in->next;
            }
        }

        triggers[trigger_count - 1] = (trigger_t) {0, 0, tr_name, type, no_buttons, buttons, no_midi_in, midi_ins};
#ifdef _DEBUG
        printf("configured %s with %d buttons and %d midi ins.\n", tr_name, no_buttons, no_midi_in);
#endif
        trigger = trigger->next;
    }
}

static void configure_actions(json *config) {

    json *actions_parent = json_get_item(config, "actions");
    if (actions_parent == NULL) {
        perror("Error: no actions are configured\n");
        exit(EXIT_FAILURE);
    }
    json *action = actions_parent->child;

    while (action) {
        action_count++;
        actions = srealloc(actions, sizeof(action_t) * action_count);
        const char *ac_name = strdup(json_get_string(action, "name", "unknown"));
        const char *ac_type = json_get_string(action, "type", "unknown");

        action_type type;
        if (strcmp(ac_type, "MIDI_OUT") == 0) {
            type = MIDI_OUT;
        } else if (strcmp(ac_type, "OTHER") == 0) {
            type = OTHER;
        } else {
            perror("Error: unknown action type");
            exit(EXIT_FAILURE);
        }

        int recurring = json_get_int(action, "recurring", 0);
        int length = json_get_int(action, "length", 0);
        int quantize = json_get_int(action, "quantize", 0);

        int no_triggers = 0;
        int *trigger_indices = NULL;
        json *trigger_parent = json_get_item(action, "triggers");
        if (trigger_parent != NULL) {
            json *trigger = trigger_parent->child;
            while (trigger) {
                no_triggers++;
                trigger_indices = srealloc(trigger_indices, sizeof(int) * no_triggers);
                int trigger_index = json_get_int(trigger, "id", 0);
                trigger_indices[no_triggers - 1] = trigger_index;
                trigger = trigger->next;
            }
        }

        action_data_t *on_data;
        json *on_data_parent = json_get_item(action, "on_data");
        if (on_data_parent == NULL) {
            perror("Error: no action ON data configured\n");
            exit(EXIT_FAILURE);
        }
        resolve_action_data(on_data_parent, &on_data, type);

        action_data_t *off_data;
        json *off_data_parent = json_get_item(action, "off_data");
        if (off_data_parent != NULL) {
            resolve_action_data(off_data_parent, &off_data, type);
            actions[action_count - 1] = (action_t) {0, ac_name, recurring, length, quantize, no_triggers, trigger_indices, type, *on_data, *off_data};
        } else {
            actions[action_count - 1] = (action_t) {0, ac_name, recurring, length, quantize, no_triggers, trigger_indices, type, *on_data};
        }

        action = action->next;
    }
}

static js_filter_type resolve_js_filter_type(const char *type) {
    js_filter_type filter_type;
    if (strcmp(type, "PITCH") == 0) {
        filter_type = PITCH_FILTER;
    } else if (strcmp(type, "MSB") == 0) {
        filter_type = MSB_FILTER;
    } else if (strcmp(type, "LSB") == 0) {
        filter_type = LSB_FILTER;
    } else {
        perror("Error: unknown joystick filter type");
        exit(EXIT_FAILURE);
    }
    return filter_type;
}

static ac_data_js_t get_js_data(json *data) {
    json *js_axis_data = json_get_item(data, "value");
    int js_index = json_get_int(js_axis_data, "js_index", 0);
    int axis_index = json_get_int(js_axis_data, "axis_index", 0);
    int inverted = json_get_int(js_axis_data, "inverted", 0);
    int chord_index = json_get_int(js_axis_data, "chord_index", 0);
    js_filter_type filter_type = resolve_js_filter_type(json_get_string(js_axis_data, "filter", "unknown"));
    return (ac_data_js_t) {js_index, axis_index, inverted, filter_type, chord_index};
}

static unsigned char get_integer_data(json *data) {
    return (unsigned char) json_get_int(data, "value", 0);
}

static ac_data_midi_in_t get_midi_data(json *data) {
    json *midi_in_data = json_get_item(data, "value");
    int port_index = json_get_int(midi_in_data, "port_index", 0);
    const char *byte_type = json_get_string(midi_in_data, "byte_type", "unknown");
    int inverted = json_get_int(midi_in_data, "inverted", 0);
    midi_byte_type midi_in_byte_type;
    if (!strcmp(byte_type, "STATUS")) {
        midi_in_byte_type = STATUS;
    } else if (!strcmp(byte_type, "MSB")) {
        midi_in_byte_type = MSB;
    } else if (!strcmp(byte_type, "LSB")) {
        midi_in_byte_type = LSB;
    } else {
        perror("Error: unknown midi in byte type");
        exit(EXIT_FAILURE);
    }
    return (ac_data_midi_in_t) {port_index, midi_in_byte_type, inverted};
}

static ac_data_type resolve_action_data_type(const char *type) {
    ac_data_type ac_type;
    if (strcmp(type, "JS_AXIS") == 0) {
        ac_type = JS_AXIS;
    } else if (strcmp(type, "MIDI_IN") == 0) {
        ac_type = MIDI_IN;
    } else if (strcmp(type, "INTEGER") == 0) {
        ac_type = INTEGER;
    } else {
        perror("Error: unknown action data type");
        exit(EXIT_FAILURE);
    }
    return ac_type;
}

void static resolve_action_data(json *json_data, action_data_t **data, action_type type) {
    int i;

    if (type == MIDI_OUT) {
        *data = smalloc(sizeof(ac_midi_data_t));
        json *json_data_child = json_data->child;
        (*data)->midi_data.port = json_get_int(json_data_child, "port_index", 0);
        for (i = 0; i < 3; i++) {
            json_data_child = json_data_child->next;
            const char *dt_type = json_get_string(json_data_child, "type", "unknown");
            ac_data_type data_type = resolve_action_data_type(dt_type);
            (*data)->midi_data.data_type[i] = data_type;
            if (data_type == JS_AXIS) {
                (*data)->midi_data.data[i].js_data = get_js_data(json_data_child);
            } else if (data_type == MIDI_IN) {
                (*data)->midi_data.data[i].midi_in_data = get_midi_data(json_data_child);
            } else if (data_type == INTEGER) {
                (*data)->midi_data.data[i].int_data = get_integer_data(json_data_child);
            }
        }
    } else /*if (type == OTHER)*/ {
        *data = smalloc(sizeof(ac_other_data_t));
        const char *dt_type = json_get_string(json_data, "type", "unknown");
        ac_data_type data_type = resolve_action_data_type(dt_type);
        (*data)->other_data.data_type = data_type;
        if (data_type == JS_AXIS) {
            (*data)->other_data.data.js_data = get_js_data(json_data);
        } else if (data_type == MIDI_IN) {
            (*data)->other_data.data.midi_in_data = get_midi_data(json_data);
        } else if (data_type == INTEGER) {
            (*data)->other_data.data.int_data = get_integer_data(json_data);
        }
    }
}

void read_config() {

    char *buffer = read_file(CONFIG_FILE);
    json *config = json_create(buffer);

    configure_triggers(config);
    configure_actions(config);

    configure_scales_and_chords(config);

    json_dispose(config);
    free(buffer);

}

