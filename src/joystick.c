#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/joystick.h>
#include <unistd.h>
#include "common.h"
#include "utils.h"
#include "joystick.h"
#include "event.h"

static void add_joystick(int index, int no_buttons, int no_axes);

extern int shutdown;
extern js_status_t *joysticks;

int count_joysticks() {
    DIR *dir;
    struct dirent *ent;

    if ((dir = opendir(INPUT_DIR)) != NULL) {
        int count = 0;
        while ((ent = readdir(dir)) != NULL) {
            if (regex_match(ent->d_name, JOYSTICK_DEV_REGEX)) {
                count++;
            }
        }
        printf("Joysticks found: %d\n", count);
        closedir(dir);
        return count;
    } else {
        perror("Error reading input device directory");
        exit(EXIT_FAILURE);
    }
}

void *thread_joystick(void *arg) {

    int joy_fd, no_axes = 0, no_buttons = 0, joy_no = *(int *) arg;
    char js[16], name[MAX_JOY_NAME_LENGTH];
    struct js_event js_e;

    sprintf(js, "%s%s%i", INPUT_DIR, JS_PREFIX, joy_no);

    joy_fd = open(js, O_RDONLY);
    handle_read_result(joy_fd, "Cannot initialize joystick");

    ioctl(joy_fd, JSIOCGAXES, &no_axes);
    ioctl(joy_fd, JSIOCGBUTTONS, &no_buttons);
    ioctl(joy_fd, JSIOCGNAME(MAX_JOY_NAME_LENGTH), &name);
#ifdef _DEBUG
    printf("%d-axis %d-button joystick opened (%s at %s)\n", no_axes, no_buttons, name, js);
#endif
    add_joystick(joy_no, no_buttons, no_axes);

    while (!shutdown) {
        if (!wait_for_input(joy_fd)) {
            continue;
        }
        handle_read_result(read(joy_fd, &js_e, sizeof(struct js_event)), "Cannot read from joystick");
        switch (js_e.type) {
            case JS_EVENT_BUTTON :
            case JS_EVENT_BUTTON + JS_EVENT_INIT :
                joysticks[joy_no].buttons[js_e.number] = js_e.value;
                notify_triggers();
#ifdef _DEBUG
                printf("there was input from joystick %s (button) %d,%d,%d,%d\n", name, js_e.number, js_e.value, js_e.type, js_e.time);
#endif
                break;
            case JS_EVENT_AXIS :
            case JS_EVENT_AXIS + JS_EVENT_INIT :
                joysticks[joy_no].axes[js_e.number] = (unsigned short) (0x8000U + js_e.value);
                notify_actions();
#ifdef _DEBUG
                printf("there was input from joystick %s (axis) %d,%d,%d,%d\n", name, js_e.number, js_e.value, js_e.type, js_e.time);
#endif
                break;
            default :
                printf("Error reading from joystick %x.\n", js_e.type);
                exit(EXIT_FAILURE);
        }
    }

    free(joysticks[joy_no].axes);
    free(joysticks[joy_no].buttons);

    close(joy_fd);

    exit_thread("joystick", joy_no, name);
}

static void add_joystick(int index, int no_buttons, int no_axes) {
    int *btns = smalloc(sizeof(int) * no_buttons);
    short *axes = smalloc(sizeof(int) * no_axes);

    joysticks[index] = (js_status_t) {no_axes, no_buttons, axes, btns, 0};
}

