#include <stdlib.h>
#include "../lib/minunit.h"
#include "utils.h"

#define MAJOR_SCALE ((scale_t) {"Major", NULL, 0b101011010101})
#define MAJOR_CHORD ((scale_t) {"Major", NULL, 0b100010010000})

static char *test_get_chord_base_tone() {
/*    char *message = smalloc(100);
    const unsigned short input = 6144;
    const unsigned short offset = 0;
    const scale_t filtered_scale = (scale_t) {NULL, NULL, 0b100001010000};

    unsigned char actual_base_tone = get_chord_base_tone(input, MAJOR_SCALE, MAJOR_CHORD, offset);
    unsigned char expected_base_tone = get_tone(input, filtered_scale, offset);

    sprintf(message, "Expected base tone was %d but the actualt base tone was %d", expected_base_tone, actual_base_tone);
    mu_assert(message, actual_base_tone == expected_base_tone);

    free(message);*/

    return 0;
}

int main(int argc, char **argv) {
    mu_run_test(test_get_chord_base_tone);
    return 0;
}
