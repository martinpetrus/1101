#ifndef COMMON_HEADER
#define COMMON_HEADER

#include <semaphore.h>
#include <math.h>

#define INPUT_DIR "/dev/input/"
#define JS_PREFIX "js"
#define SND_DIR "/dev/snd/"
#define JOYSTICK_DEV_REGEX "js[0-9]*"
#define MIDI_DEV_REGEX "midiC[0-9]D[0-9]"

#define smalloc(size) ({ void *p; p = malloc(size); if (!p) { perror("Out of memory"); exit(EXIT_FAILURE); } p; })
#define srealloc(ptr, size) ({ void *p; p = realloc(ptr, size); if (!p) { perror("Out of memory"); exit(EXIT_FAILURE); } p; })

#define handle_result(res, msg) if (res != 0) { perror(msg); exit(EXIT_FAILURE); }
#define handle_read_result(res, msg) if (res == -1) { perror(msg); exit(EXIT_FAILURE); }

#define GET_MSB(val) ((unsigned char) (val >> 9))
#define GET_LSB(val) ((unsigned char) (val & 0x1FC >> 2))
#define POPCNT(val) (__builtin_popcount(val))

typedef enum {
    STATUS,
    MSB,
    LSB
} midi_byte_type;

typedef struct {
    const char* name;
    const char* value;
    unsigned short mask : 12;
    int tones;
    unsigned char *tone_positions;
} scale_t;

typedef enum {
    MIDI_OUT,
//    change_BPM,
//    change_signature,
//    change_time,
//    change_key,
//    change_scale,
//    loop_record,
//    loop_play,
    OTHER
} action_type;

typedef struct {
    unsigned char status;
    unsigned char data[2];
    int flag;
} midi_msg_t;

typedef union {
    midi_msg_t midi_data;
} off_ac_data_t;

typedef struct {
    action_type type;
    off_ac_data_t data;
} off_ac_t;

typedef enum {
    JS_AXIS,
    MIDI_IN,
    INTEGER
} ac_data_type;

typedef enum {
    MSB_FILTER,
    LSB_FILTER,
    PITCH_FILTER
} js_filter_type;

typedef struct {
    int js_index;
    int axis_index;
    int inverted;
    js_filter_type filter;
    int chord_index;
} ac_data_js_t;

typedef struct {
    int port_index;
    midi_byte_type byte_type;
    int inverted;
} ac_data_midi_in_t;

typedef union {
    ac_data_js_t js_data;
    ac_data_midi_in_t midi_in_data;
    unsigned char int_data;
} ac_data_t;

typedef struct {
    int port;
    ac_data_type data_type[3];
    ac_data_t data[3];
} ac_midi_data_t;

typedef struct {
    ac_data_type data_type;
    ac_data_t data;
} ac_other_data_t;

typedef union {
    ac_midi_data_t midi_data;
    ac_other_data_t other_data;
} action_data_t;

typedef struct {
    int status;
    const char *name;
    int recurring;
    int length; //0 if waiting for trigger off
    int quantize; //0 if realtime
    int no_triggers;
    int *trigger_indices;
    action_type type;
    action_data_t on_data;
    action_data_t off_data;
} action_t;

typedef struct {
    int no_axes;
    int no_buttons;
    unsigned short *axes;
    int *buttons;
    int flag;
} js_status_t;

typedef enum {
    PRESS_RELEASE,
    TWO_WAY
} trigger_type;

typedef struct {
    int js_index;
    int btn_index;
    int btn_value;
} tr_btn_t;

typedef struct {
    int port_index;
    unsigned char status_lo;
    unsigned char status_hi;
    unsigned char msb_lo;
    unsigned char msb_hi;
    unsigned char lsb_lo;
    unsigned char lsb_hi;
} tr_midi_t;

typedef struct {
    int status;
    int current_state;
    const char *name;
    trigger_type type;
    int no_buttons;
    tr_btn_t *buttons;
    int no_midi_in;
    tr_midi_t *midi_ins;
//    int rnd_trigger;
} trigger_t;

typedef struct {
    int chord_count;
    scale_t *chords;
    int scale_count;
    scale_t *scales;
    scale_t ***scale_chord_offset_map;
    int scale_index;
    int scale_offset;
} context_t;

#endif