#ifndef MINUNIT
#define MINUNIT

#include <stdio.h>

#define mu_assert(message, test) do { if (!(test)) return message; } while (0)
#define mu_run_test(test) do { char *message = test(); if (message) { printf("Test '%s' failed: %s\n", #test, message); return 1; } } while (0)

#endif
