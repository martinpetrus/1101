#ifndef MIDI_HEADER
#define MIDI_HEADER

void init_midi_ports();
void *thread_midi_in(void *arg);
void *thread_midi_out(void *arg);
void send_message(midi_msg_t msg, int port_no);

#endif