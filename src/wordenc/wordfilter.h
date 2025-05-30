#pragma once

#include <stdint.h>

#include "../jagfile.h"

// name taken from rsc
typedef struct {
    int *fragments;
    char **badWords;
    int8_t ***badCombinations;
    char **domains;
    char **tlds;
    int *tldType;
} WordFilter;

void wordfilter_unpack(Jagfile *jag);
const char *wordfilter_filter(char *input);
