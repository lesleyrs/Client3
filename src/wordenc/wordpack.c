#include <stdbool.h>
#include <string.h>

#include "../defines.h"
#include "../packet.h"
#include "../platform.h"
#include "wordpack.h"

static char charBuffer[100] = {0};
static const char TABLE[] = {
    // combined to save space:
    ' ', 'e', 't', 'a', 'o', 'i', 'h', 'n', 's', 'r', 'd', 'l', 'u',
    // allowed:
    'm', 'w', 'c', 'y', 'f', 'g', 'p', 'b', 'v', 'k', 'x', 'j', 'q', 'z',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    // NOTE disabled pound, doesn't fit in C char
    ' ', '!', '?', '.', ',', ':', ';', '(', ')', '-', '&', '*', '\\', '\'', '@', '#', '+', '=', 'A', '$', '%', '"', '[', ']'
    // ' ', '!', '?', '.', ',', ':', ';', '(', ')', '-', '&', '*', '\\', '\'', '@', '#', '+', '=', '£', '$', '%', '"', '[', ']'
};
static const int TABLE_LENGTH = 61;

char *wordpack_unpack(Packet *word, int length) {
    int pos = 0;
    int carry = -1;

    int nibble;
    for (int i = 0; i < length; i++) {
        int value = g1(word);
        nibble = value >> 4 & 0xf;

        if (carry != -1) {
            charBuffer[pos++] = TABLE[(carry << 4) + nibble - 195];
            carry = -1;
        } else if (nibble < 13) {
            charBuffer[pos++] = TABLE[nibble];
        } else {
            carry = nibble;
        }

        nibble = value & 0xf;
        if (carry != -1) {
            charBuffer[pos++] = TABLE[(carry << 4) + nibble - 195];
            carry = -1;
        } else if (nibble < 13) {
            charBuffer[pos++] = TABLE[nibble];
        } else {
            carry = nibble;
        }
    }

    bool uppercase = true;
    for (int i = 0; i < pos; i++) {
        char c = charBuffer[i];
        if (uppercase && c >= 'a' && c <= 'z') {
            charBuffer[i] = (char)(charBuffer[i] - 32);
            uppercase = false;
        }

        if (c == '.' || c == '!') {
            uppercase = true;
        }
    }
    return platform_strndup(charBuffer, pos);
}

void wordpack_pack(Packet *word, char *str) {
    if (strlen(str) > CHAT_LENGTH) {
        str = substring(str, 0, CHAT_LENGTH);
    }
    strtolower(str);

    int carry = -1;
    for (size_t i = 0; i < strlen(str); i++) {
        char c = str[i];

        int index = 0;
        for (int j = 0; j < TABLE_LENGTH; j++) {
            if (c == TABLE[j]) {
                index = j;
                break;
            }
        }

        if (index > 12) {
            index += 195;
        }

        if (carry == -1) {
            if (index < 13) {
                carry = index;
            } else {
                p1(word, index);
            }
        } else if (index < 13) {
            p1(word, (carry << 4) + index);
            carry = -1;
        } else {
            p1(word, (carry << 4) + (index >> 4));
            carry = index & 0xf;
        }
    }

    if (carry != -1) {
        p1(word, carry << 4);
    }
}
