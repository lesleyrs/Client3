#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../defines.h"
#include "../platform.h"
#include "jstring.h"

static char builder[USERNAME_LENGTH + 1] = {0};
static const char BASE37_LOOKUP[] = {
    '_', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i',
    'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's',
    't', 'u', 'v', 'w', 'x', 'y', 'z',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};

int64_t jstring_to_base37(char *str) {
    int64_t hash = 0L;

    for (size_t i = 0; i < strlen(str) && i < 12; i++) {
        char c = str[i];
        hash *= 37L;

        if (c >= 'A' && c <= 'Z') {
            hash += c + 1 - 65;
        } else if (c >= 'a' && c <= 'z') {
            hash += c + 1 - 97;
        } else if (c >= '0' && c <= '9') {
            hash += c + 27 - 48;
        }
    }

    while (hash % 37L == 0L && hash != 0L) {
        hash /= 37L;
    }

    return hash;
}

char *jstring_from_base37(int64_t username) {
    // >= 37 to the 12th power
    if (username <= 0L || username >= 6582952005840035281L) {
        return platform_strdup("invalid_name");
    }

    if (username % 37L == 0L) {
        return platform_strdup("invalid_name");
    }

    int len = 0;
    while (username != 0L) {
        int64_t last = username;
        username /= 37L;
        builder[11 - len++] = BASE37_LOOKUP[(int)(last - username * 37L)];
    }

    return platform_strndup(builder + 12 - len, len);
}

int64_t jstring_hash_code(char *str) {
    strtoupper(str);
    int64_t hash = 0L;

    for (size_t i = 0; i < strlen(str); i++) {
        hash = hash * 61L + (int64_t)str[i] - 32L;
        hash = hash + (hash >> 56) & 0xffffffffffffffl;
    }

    return hash;
}

char *jstring_format_ipv4(int ip) {
    char *buf = calloc(16, sizeof(char));
    sprintf(buf, "%d.%d.%d.%d", (ip >> 24 & 0xff), (ip >> 16 & 0xff), (ip >> 8 & 0xff), (ip & 0xff));
    return buf;
}

char *jstring_format_name(char *str) {
    if (strlen(str) == 0) {
        return str;
    }

    for (size_t i = 0; i < strlen(str); i++) {
        if (str[i] == '_') {
            str[i] = ' ';

            if (i + 1 < strlen(str) && str[i + 1] >= 'a' && str[i + 1] <= 'z') {
                str[i + 1] = (char)(str[i + 1] + 'A' - 97);
            }
        }
    }

    if (str[0] >= 'a' && str[0] <= 'z') {
        str[0] = (char)(str[0] + 'A' - 97);
    }

    return str;
}

void jstring_to_sentence_case(char *str) {
    strtolower(str);
    size_t length = strlen(str);
    bool capitalize = true;

    for (size_t i = 0; i < length; i++) {
        char c = str[i];

        if (capitalize && c >= 'a' && c <= 'z') {
            str[i] = (char)(str[i] - 32);
            capitalize = false;
        }

        if (c == '.' || c == '!') {
            capitalize = true;
        }
    }
}

char *jstring_to_asterisk(char *str) {
    const size_t length = strlen(str);
    char *temp = malloc(length + 1);
    memset(temp, '*', length);
    temp[length] = '\0';
    return temp;
}
