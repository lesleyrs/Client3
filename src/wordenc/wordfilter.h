#pragma once

#include <stdint.h>

#include "../jagfile.h"

// name taken from rsc
typedef struct {
    int *fragments;
    char **badWords;
    int8_t (**badCombinations)[2];
    char **domains;
    char **tlds;
    int *tldType;
} WordFilter;

void wordfilter_unpack(Jagfile *jag);
void wordfilter_filter(char *input);
void wordfilter_free_global(void);
