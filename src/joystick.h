#ifndef JOYSTICK_HEADER
#define JOYSTICK_HEADER

#define MAX_JOY_NAME_LENGTH 64

int count_joysticks();
void *thread_joystick(void *);

#endif