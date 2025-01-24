#pragma once

#include <stdint.h>

// name and packaging confirmed 100% in rs2/mapview applet strings
// later repackaged under jagex2/jstring

int64_t jstring_to_base37(char *str);
char *jstring_from_base37(int64_t username);
int64_t jstring_hash_code(char *str);
char *jstring_format_ipv4(int ip);
char *jstring_format_name(char *str);
void jstring_to_sentence_case(char *str);
char *jstring_to_asterisk(char *str);
