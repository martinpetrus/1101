#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include "common.h"
#include "midi.h"
#include "utils.h"
#include "event.h"

extern int shutdown;
extern int ext_sync;
extern int timer_time;
extern int midi_in_count;
extern int midi_out_count;
extern midi_msg_t *midi_in, *midi_out;
extern sem_t *midi_out_put_sems, *midi_out_take_sems;

int *midi_in_ports;
int *midi_out_ports;

void init_midi_ports() {

    midi_in_count = 0;
    midi_out_count = 0;
    midi_in_ports = NULL;
    midi_out_ports = NULL;

    DIR *dir;
    char midi[32];

    struct dirent *ent;
    if ((dir = opendir(SND_DIR)) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            if (regex_match(ent->d_name, MIDI_DEV_REGEX)) {
                sprintf(midi, "%s%s", SND_DIR, ent->d_name);
                int midi_in_fd = open(midi, O_RDONLY);
                if (midi_in_fd != -1) {
                    midi_in_count++;
                    midi_in_ports = srealloc(midi_in_ports, sizeof(int) * midi_in_count);
                    midi_in_ports[midi_in_count - 1] = midi_in_fd;
                }
                int midi_out_fd = open(midi, O_WRONLY);
                if (midi_out_fd != -1) {
                    midi_out_count++;
                    midi_out_ports = srealloc(midi_out_ports, sizeof(int) * midi_out_count);
                    midi_out_ports[midi_out_count - 1] = midi_out_fd;
                }
            }
        }
        closedir(dir);
        printf("Midi in ports found: %d\n", midi_in_count);
        printf("Midi out ports found: %d\n", midi_out_count);

    }
}

void *thread_midi_in(void *arg) {
    int port_no = *(int *) arg;
    int fd = midi_in_ports[port_no];

    unsigned char midi_in_byte;
    midi_msg_t midi_in_msg;
    int databyte_index = 0;

    while (!shutdown) {
        if (!wait_for_input(fd)) {
            continue;
        }
        handle_read_result(read(fd, &midi_in_byte, sizeof(char)), "Cannot read from MIDI in");

        if (midi_in_byte >> 7) { // a status byte
            if (midi_in_byte == 0xF8) {
                ext_sync = 1;
            } else {
                midi_in_msg = (midi_msg_t) {midi_in_byte, 0, 0, 1};
            }
        } else {
            midi_in_msg.data[databyte_index] = midi_in_byte;
            if (midi_in_msg.status >> 5 != 0x6 && !databyte_index) { // not 0xC nor 0xD - a LSB expected
                databyte_index = 1;
                continue;
            }
            databyte_index = 0;
            midi_in[port_no] = midi_in_msg;
#ifdef _DEBUG
            printf("(%d) received %d-%d-%d on port %d\n", timer_time, midi_in_msg.status, midi_in_msg.data[0], midi_in_msg.data[1], port_no);
#endif
            notify_triggers();
        }
    }

    close(fd);

    exit_thread("midi in", port_no, "");
}

void *thread_midi_out(void *arg) {
    int port_no = *(int *) arg;
    int fd = midi_out_ports[port_no];
    
    unsigned char last_status = 0;
    size_t msg_length;
    char msg[3];

    while (!shutdown) {
        if (wait_for_sem(&midi_out_take_sems[port_no]) == -1) {
            continue;
        }

        msg_length = 1;
        unsigned char status = midi_out[port_no].status;

        if (status == 0xF8) {
            msg[0] = status;
#ifdef _DEBUG
            printf("(%d) sending F8 on port %d\n", timer_time, port_no);
#endif
        } else {
            if (last_status != status) {
                last_status = status;
                msg[0] = status;
                msg_length++;
            }
            msg[msg_length - 1] = midi_out[port_no].data[0];
            if (status >> 5 != 0x6) {
                msg[msg_length] = midi_out[port_no].data[1];
                msg_length++;
            }
#ifdef _DEBUG
            printf("(%d) sending %d-%d-%d on port %d\n", timer_time, last_status, midi_out[port_no].data[0], msg_length == 2 ? midi_out[port_no].data[1] : 0, port_no);
#endif
        }

        ssize_t res = write(fd, msg, msg_length);
        if (res == -1) {
            perror("Cannot write to MIDI out");
            exit(EXIT_FAILURE);
        }

        sem_post(&midi_out_put_sems[port_no]);
    }

    close(fd);

    exit_thread("midi out", port_no, "");
}


void send_message(midi_msg_t msg, int port_no) {
    sem_wait(&midi_out_put_sems[port_no]);
    midi_out[port_no] = msg;
    sem_post(&midi_out_take_sems[port_no]);
}
