#pragma once

#include "../packet.h"

// name taken from rsc

char *wordpack_unpack(Packet *word, int length);
void wordpack_pack(Packet *word, char *str);
