#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "wordfilter.h"
#include "../packet.h"
#include "../platform.h"

static void wordfilter_read(Packet *bad, Packet *domain, Packet *fragments, Packet *tld);
static void readTld(Packet *buf);
static void readBadWords(Packet *buf);
static void readDomains(Packet *buf);
static void readFragments(Packet *buf);
static void readBadCombinations(Packet *buf, char **badwords, int8_t (**badCombinations)[2]);
static void readDomain(Packet *buf, char **domains);
static void filterCharacters(char *in);
static bool allowCharacter(char c);
static void replaceUpperCases(char *in, char *unfiltered);
static void formatUpperCases(char *in);
static void filterBad(char *in);
static void filterDomains(char *in);
static void filterDomain(char *filteredDot, char *filteredAt, char *domain, char *in);
static int getDomainAtFilterStatus(int end, char *a, char *b);
static int getDomainDotFilterStatus(char *a, char *b, size_t start);
static void filterTld(char *in);
static void filterTld2(char *filteredSlash, int type, char *chars, char *tld, char *filteredDot);
static int getTldDotFilterStatus(char *a, char *b, int start);
static int getTldSlashFilterStatus(char *b, size_t end, char *a);
static void filter(int combos_count, int8_t (*badCombinations)[2], char *chars, char *fragment);
static bool comboMatches(int8_t a, int8_t (*combos)[2], int combos_count, int8_t b);
static int getEmulatedDomainCharSize(char c, char a, char b);
static int getEmulatedSize(char c, char a, char b);
static int8_t getIndex(char c);
static void filterFragments(char *chars);
static int indexOfNumber(char *input, int off);
static int indexOfNonNumber(int off, char *input);
static bool isLowerCaseAlpha(char c);
static bool isBadFragment(char *input);
static int firstFragmentId(char *chars);

static const char* ALLOWLIST[] = { "cook", "cook's", "cooks", "seeks", "sheet" };
#define ALLOWLIST_COUNT 5
static int badwords_count = 0;
static int domains_count = 0;
static int fragments_count = 0;
static int tlds_count = 0;
static int *combos_count = NULL;

WordFilter _WordFilter;

void wordfilter_unpack(Jagfile *jag) {
	Packet *fragments = jagfile_to_packet(jag, "fragmentsenc.txt");
	Packet *bad = jagfile_to_packet(jag, "badenc.txt");
	Packet *domain = jagfile_to_packet(jag, "domainenc.txt");
	Packet *tld = jagfile_to_packet(jag, "tldlist.txt");
	wordfilter_read(bad, domain, fragments, tld);
}

static void wordfilter_read(Packet *bad, Packet *domain, Packet *fragments, Packet *tld) {
	readBadWords(bad);
	readDomains(domain);
	readFragments(fragments);
	readTld(tld);
}

static void readTld(Packet *buf) {
	tlds_count = g4(buf);
	_WordFilter.tlds = calloc(tlds_count, sizeof(char*));
	_WordFilter.tldType = calloc(tlds_count, sizeof(int));
	for (int i = 0; i < tlds_count; i++) {
		_WordFilter.tldType[i] = g1(buf);
		int tld_count = g1(buf);
		char *tld = calloc(tld_count + 1, sizeof(char));
		for (int j = 0; j < tld_count; j++) {
			tld[j] = (char) g1(buf);
		}
		_WordFilter.tlds[i] = tld;
	}
}

static void readBadWords(Packet *buf) {
	badwords_count = g4(buf);
	combos_count = calloc(badwords_count, sizeof(int));
	_WordFilter.badWords = calloc(badwords_count, sizeof(char*));
	_WordFilter.badCombinations = calloc(badwords_count, sizeof(_WordFilter.badCombinations));
	readBadCombinations(buf, _WordFilter.badWords, _WordFilter.badCombinations);
}

static void readDomains(Packet *buf) {
	domains_count = g4(buf);
	_WordFilter.domains = calloc(domains_count, sizeof(char*));
	readDomain(buf, _WordFilter.domains);
}

static void readFragments(Packet *buf) {
	fragments_count = g4(buf);
	_WordFilter.fragments = calloc(fragments_count, sizeof(int));
	for (int i = 0; i < fragments_count; i++) {
		_WordFilter.fragments[i] = g2(buf);
	}
}

static void readBadCombinations(Packet *buf, char **badwords, int8_t (**badCombinations)[2]) {
	for (int i = 0; i < badwords_count; i++) {
		int badword_count = g1(buf);
		char *badword = calloc(badword_count + 1, sizeof(char));
		for (int j = 0; j < badword_count; j++) {
			badword[j] = (char) g1(buf);
		}
		badwords[i] = badword;
		combos_count[i] = g1(buf);
		int8_t (*combination)[2] = calloc(combos_count[i], sizeof(*combination));
		for (int j = 0; j < combos_count[i]; j++) {
			combination[j][0] = (int8_t) g1(buf);
			combination[j][1] = (int8_t) g1(buf);
		}
		if (combos_count[i] > 0) {
			badCombinations[i] = combination;
		}
	}
}

static void readDomain(Packet *buf, char **domains) {
	for (int i = 0; i < domains_count; i++) {
		int domain_count = g1(buf);
		char *domain = calloc(domain_count + 1, sizeof(char));
		for (int j = 0; j < domain_count; j++) {
			domain[j] = (char) g1(buf);
		}
		domains[i] = domain;
	}
}

static void filterCharacters(char *in) {
	size_t len = strlen(in);
	int pos = 0;
	for (size_t i = 0; i < len; i++) {
		if (allowCharacter(in[i])) {
			in[pos] = in[i];
		} else {
			in[pos] = ' ';
		}
		if (pos == 0 || in[pos] != ' ' || in[pos - 1] != ' ') {
			pos++;
		}
	}
	for (size_t i = pos; i < len; i++) {
		in[i] = ' ';
	}
}

static bool allowCharacter(char c) {
	return isprint(c) || isspace(c);
	// return c >= ' ' && c <= '\u007f' || c == ' ' || c == '\n' || c == '\t' || c == '£' || c == '€';
}

char* wordfilter_filter(char* input) {
	// uint64_t start = rs2_now();
	filterCharacters(input);
	strtrim(input);
	char *trimmed = malloc(strlen(input) + 1);
	strcpy(trimmed, input);
	strtolower(input);
	filterTld(input);
	filterBad(input);
	filterDomains(input);
	filterFragments(input);
	int j;
	for (int i = 0; i < ALLOWLIST_COUNT; i++) {
		j = -1;
		while ((j = indexof(input, ALLOWLIST[i] + j + 1)) != -1) {
			const char *allowed = ALLOWLIST[i];
			memcpy(input + j, allowed, strlen(allowed));
		}
	}
	replaceUpperCases(input, trimmed);
	formatUpperCases(input);
	// uint64_t end = rs2_now();
	strtrim(input);
	return platform_strdup(input);
}

static void replaceUpperCases(char *in, char *unfiltered) {
	int len = strlen(unfiltered);
	for (int i = 0; i < len; i++) {
		if (in[i] != '*' && isupper(unfiltered[i])) {
			in[i] = unfiltered[i];
		}
	}
}

static void formatUpperCases(char *in) {
	int len = strlen(in);
	bool upper = true;
	for (int i = 0; i < len; i++) {
		char c = in[i];
		if (!isalpha(c)) {
			upper = true;
		} else if (upper) {
			if (islower(c)) {
				upper = false;
			}
		} else if (isupper(c)) {
			in[i] = (char) (c + 'a' - 65);
		}
	}
}

static void filterBad(char *in) {
	for (int passes = 0; passes < 2; passes++) {
		for (int i = badwords_count - 1; i >= 0; i--) {
			filter(combos_count[i], _WordFilter.badCombinations[i], in, _WordFilter.badWords[i]);
		}
	}
}

static void filterDomains(char *in) {
	size_t len = strlen(in);
	char *filteredAt = malloc(len + 1);
	strcpy(filteredAt, in);
	char at[] = { '(', 'a', ')', '\0' };
	filter(0, NULL, filteredAt, at);
	char *filteredDot = malloc(len + 1);
	strcpy(filteredDot, in);
	char dot[] = { 'd', 'o', 't', '\0' };
	filter(0, NULL, filteredDot, dot);
	for (int i = domains_count - 1; i >= 0; i--) {
		filterDomain(filteredDot, filteredAt, _WordFilter.domains[i], in);
	}
}

static void filterDomain(char *filteredDot, char *filteredAt, char *domain, char *in) {
	size_t in_len = strlen(in);
	size_t domain_len = strlen(domain);

	if (domain_len <= in_len) {
		int stride;
		for (size_t start = 0; start <= in_len - domain_len; start += stride) {
			size_t end = start;
			size_t offset = 0;
			stride = 1;
			bool match;
			while (true) {
				while (true) {
					if (end >= in_len) {
						goto filter_pass;
					}
					match = false;
					char b = in[end];
					char c = 0;
					if (end + 1 < in_len) {
						c = in[end + 1];
					}
					int charSize;
					if (offset < domain_len && (charSize = getEmulatedDomainCharSize(c, domain[offset], b)) > 0) {
						end += charSize;
						offset++;
					} else {
						if (offset == 0) {
							goto filter_pass;
						}
						int charSize2;
						if ((charSize2 = getEmulatedDomainCharSize(c, domain[offset - 1], b)) > 0) {
							end += charSize2;
							if (offset == 1) {
								stride++;
							}
						} else {
							if (offset >= domain_len || isalnum(b)) {
								goto filter_pass;
							}
							end++;
						}
					}
				}
			}
			filter_pass:
			if (offset >= domain_len) {
				match = false;
				int atFilter = getDomainAtFilterStatus(start, in, filteredAt);
				int dotFilter = getDomainDotFilterStatus(in, filteredDot, end - 1);
				if (atFilter > 2 || dotFilter > 2) {
					match = true;
				}
				if (match) {
					for (size_t i = start; i < end; i++) {
						in[i] = '*';
					}
				}
			}
		}
	}
}

static int getDomainAtFilterStatus(int end, char *a, char *b) {
	if (end == 0) {
		return 2;
	}
	for (int i = end - 1; i >= 0 && !isalnum(a[i]); i--) {
		if (a[i] == '@') {
			return 3;
		}
	}
	int asteriskCount = 0;
	for (int i = end - 1; i >= 0 && !isalnum(b[i]); i--) {
		if (b[i] == '*') {
			asteriskCount++;
		}
	}
	if (asteriskCount >= 3) {
		return 4;
	} else if (!isalnum(a[end - 1])) {
		return 1;
	} else {
		return 0;
	}
}

static int getDomainDotFilterStatus(char *a, char *b, size_t start) {
	size_t len = strlen(a);
	if (start + 1 == len) {
		return 2;
	} else {
		size_t i = start + 1;
		while (true) {
			if (i < len && !isalnum(a[i])) {
				if (a[i] != '.' && a[i] != ',') {
					i++;
					continue;
				}
				return 3;
			}
			int asteriskCount = 0;
			for (size_t j = start + 1; j < len && !isalnum(b[j]); j++) {
				if (b[j] == '*') {
					asteriskCount++;
				}
			}
			if (asteriskCount >= 3) {
				return 4;
			}
			if (!isalnum(a[start + 1])) {
				return 1;
			}
			return 0;
		}
	}
}

static void filterTld(char *in) {
	size_t in_len = strlen(in);
	char *filteredDot = malloc(in_len + 1);
	strcpy(filteredDot, in);
	char dot[] = { 'd', 'o', 't', '\0' };
	filter(0, NULL, filteredDot, dot);
	char *filteredSlash = malloc(in_len + 1);
	strcpy(filteredSlash, in);
	char slash[] = { 's', 'l', 'a', 's', 'h', '\0' };
	filter(0, NULL, filteredSlash, slash);
	for (int i = 0; i < tlds_count; i++) {
		filterTld2(filteredSlash, _WordFilter.tldType[i], in, _WordFilter.tlds[i], filteredDot);
	}
}

static void filterTld2(char *filteredSlash, int type, char *chars, char *tld, char *filteredDot) {
	size_t chars_len = strlen(chars);
	size_t tld_len = strlen(tld);
	int stride;
	if (tld_len <= chars_len) {
		// bool compare = true;
		for (size_t start = 0; start <= chars_len - tld_len; start += stride) {
			size_t end = start;
			size_t offset = 0;
			stride = 1;
			bool match;
			while (true) {
				while (true) {
					if (end >= chars_len) {
						goto filter_pass;
					}
					match = false;
					char b = chars[end];
					char c = 0;
					if (end + 1 < chars_len) {
						c = chars[end + 1];
					}
					int charLen;
					if (offset < tld_len && (charLen = getEmulatedDomainCharSize(c, tld[offset], b)) > 0) {
						end += charLen;
						offset++;
					} else {
						if (offset == 0) {
							goto filter_pass;
						}
						int charLen2;
						if ((charLen2 = getEmulatedDomainCharSize(c, tld[offset - 1], b)) > 0) {
							end += charLen2;
							if (offset == 1) {
								stride++;
							}
						} else {
							if (offset >= tld_len || isalnum(b)) {
								goto filter_pass;
							}
							end++;
						}
					}
				}
			}
			filter_pass:
			if (offset >= tld_len) {
				match = false;
				int status0 = getTldDotFilterStatus(chars, filteredDot, start);
				int status1 = getTldSlashFilterStatus(filteredSlash, end - 1, chars);
				if (type == 1 && status0 > 0 && status1 > 0) {
					match = true;
				}
				if (type == 2 && ((status0 > 2 && status1 > 0) || (status0 > 0 && status1 > 2))) {
					match = true;
				}
				if (type == 3 && status0 > 0 && status1 > 2) {
					match = true;
				}
				// bool lastCheck = type == 3 && status0 > 2 && status1 > 0;
				if (match) {
					int first = start;
					int last = end - 1;
					bool findStart;
					int i;
					if (status0 > 2) {
						if (status0 == 4) {
							findStart = false;
							for (i = start - 1; i >= 0; i--) {
								if (findStart) {
									if (filteredDot[i] != '*') {
										break;
									}
									first = i;
								} else if (filteredDot[i] == '*') {
									first = i;
									findStart = true;
								}
							}
						}
						findStart = false;
						for (i = first - 1; i >= 0; i--) {
							if (findStart) {
								if (!isalnum(chars[i])) {
									break;
								}
								first = i;
							} else if (isalnum(chars[i])) {
								findStart = true;
								first = i;
							}
						}
					}
					if (status1 > 2) {
						if (status1 == 4) {
							findStart = false;
							for (i = last + 1; i < (int)chars_len; i++) {
								if (findStart) {
									if (filteredSlash[i] != '*') {
										break;
									}
									last = i;
								} else if (filteredSlash[i] == '*') {
									last = i;
									findStart = true;
								}
							}
						}
						findStart = false;
						for (i = last + 1; i < (int)chars_len; i++) {
							if (findStart) {
								if (!isalnum(chars[i])) {
									break;
								}
								last = i;
							} else if (isalnum(chars[i])) {
								findStart = true;
								last = i;
							}
						}
					}
					for (int j = first; j <= last; j++) {
						chars[j] = '*';
					}
				}
			}
		}
	}
}

static int getTldDotFilterStatus(char *a, char *b, int start) {
	if (start == 0) {
		return 2;
	}
	int i = start - 1;
	while (true) {
		if (i >= 0 && !isalnum(a[i])) {
			if (a[i] != ',' && a[i] != '.') {
				i--;
				continue;
			}
			return 3;
		}
		int asteriskCount = 0;
		int j;
		for (j = start - 1; j >= 0 && !isalnum(b[j]); j--) {
			if (b[j] == '*') {
				asteriskCount++;
			}
		}
		if (asteriskCount >= 3) {
			return 4;
		}
		if (!isalnum(a[start - 1])) {
			return 1;
		}
		return 0;
	}
}

static int getTldSlashFilterStatus(char *b, size_t end, char *a) {
	size_t len = strlen(a);
	if (end + 1 == len) {
		return 2;
	}
	size_t i = end + 1;
	while (true) {
		if (i < len && !isalnum(a[i])) {
			if (a[i] != '\\' && a[i] != '/') {
				i++;
				continue;
			}
			return 3;
		}
		int asteriskCount = 0;
		for (size_t j = end + 1; j < len && !isalnum(b[j]); j++) {
			if (b[j] == '*') {
				asteriskCount++;
			}
		}
		if (asteriskCount >= 5) {
			return 4;
		}
		if (!isalnum(a[end + 1])) {
			return 1;
		}
		return 0;
	}
}

static void filter(int combos_count_index, int8_t (*badCombinations)[2], char *chars, char *fragment) {
	size_t chars_len = strlen(chars);
	size_t fragment_len = strlen(fragment);
	if (fragment_len <= chars_len) {
		// bool compare = true;
		int stride;
		for (int start = 0; start <= (int)(chars_len - fragment_len); start += stride) {
			size_t end = start;
			size_t fragOff = 0;
			int iterations = 0;
			stride = 1;
			bool isSymbol = false;
			bool isEmulated = false;
			bool isNumeral = false;
			bool bad;
			char b;
			char c;
			while (true) {
				while (true) {
					if (end >= chars_len || (isEmulated && isNumeral)) {
						goto label163;
					}
					bad = false;
					b = chars[end];
					c = '\0';
					if (end + 1 < chars_len) {
						c = chars[end + 1];
					}
					int charLen;
					if (fragOff < fragment_len && (charLen = getEmulatedSize(c, fragment[fragOff], b)) > 0) {
						if (charLen == 1 && isdigit(b)) {
							isEmulated = true;
						}
						if (charLen == 2 && (isdigit(b) || isdigit(c))) {
							isEmulated = true;
						}
						end += charLen;
						fragOff++;
					} else {
						if (fragOff == 0) {
							goto label163;
						}
						int charLen2;
						if ((charLen2 = getEmulatedSize(c, fragment[fragOff - 1], b)) > 0) {
							end += charLen2;
							if (fragOff == 1) {
								stride++;
							}
						} else {
							if (fragOff >= fragment_len || !isLowerCaseAlpha(b)) {
								goto label163;
							}
							if (!isalnum(b) && b != '\'') {
								isSymbol = true;
							}
							if (isdigit(b)) {
								isNumeral = true;
							}
							end++;
							iterations++;
							if (iterations * 100 / (end - start) > 90) {
								goto label163;
							}
						}
					}
				}
			}
			label163:
			if (fragOff >= fragment_len && (!isEmulated || !isNumeral)) {
				bad = true;
				int cur;
				if (isSymbol) {
					bool badCurrent = false;
					bool badNext = false;
					if (start - 1 < 0 || (!isalnum(chars[start - 1]) && chars[start - 1] != '\'')) {
						badCurrent = true;
					}
					if (end >= chars_len || (!isalnum(chars[end]) && chars[end] != '\'')) {
						badNext = true;
					}
					if (!badCurrent || !badNext) {
						bool good = false;
						cur = start - 2;
						if (badCurrent) {
							cur = start;
						}
						while (!good && cur < (int)end) {
							if (cur >= 0 && (isalnum(chars[cur]) || chars[cur] == '\'')) {
								char frag[3];
								int off;
								for (off = 0; off < 3 && cur + off < (int)chars_len && (isalnum(chars[cur + off]) || chars[cur + off] == '\''); off++) {
									frag[off] = chars[cur + off];
								}
								bool valid = off != 0;
								if (off < 3 && cur - 1 >= 0 && (isalnum(chars[cur - 1]) || chars[cur - 1] == '\'')) {
									valid = false;
								}
								if (valid && !isBadFragment(frag)) {
									good = true;
								}
							}
							cur++;
						}
						if (!good) {
							bad = false;
						}
					}
				} else {
					b = ' ';
					if (start - 1 >= 0) {
						b = chars[start - 1];
					}
					c = ' ';
					if (end < chars_len) {
						c = chars[end];
					}
					int8_t bIndex = getIndex(b);
					int8_t cIndex = getIndex(c);
					if (badCombinations && comboMatches(bIndex, badCombinations, combos_count_index, cIndex)) {
						bad = false;
					}
				}
				if (bad) {
					int numeralCount = 0;
					int alphaCount = 0;
					for (size_t n = start; n < end; n++) {
						if (isdigit(chars[n])) {
							numeralCount++;
						} else if (isalpha(chars[n])) {
							alphaCount++;
						}
					}
					if (numeralCount <= alphaCount) {
						for (cur = start; cur < (int)end; cur++) {
							chars[cur] = '*';
						}
					}
				}
			}
		}
	}
}

static bool comboMatches(int8_t a, int8_t (*combos)[2], int combos_count_index, int8_t b) {
	int first = 0;
	if (combos[first][0] == a && combos[first][1] == b) {
		return true;
	}
	int last = combos_count_index - 1;
	if (combos[last][0] == a && combos[last][1] == b) {
		return true;
	}
	do {
		int middle = (first + last) / 2;
		if (combos[middle][0] == a && combos[middle][1] == b) {
			return true;
		}
		if (a < combos[middle][0] || (a == combos[middle][0] && b < combos[middle][1])) {
			last = middle;
		} else {
			first = middle;
		}
	} while (first != last && first + 1 != last);
	return false;
}

static int getEmulatedDomainCharSize(char c, char a, char b) {
	if (a == b) {
		return 1;
	} else if (a == 'o' && b == '0') {
		return 1;
	} else if (a == 'o' && b == '(' && c == ')') {
		return 2;
	} else if (a == 'c' && (b == '(' || b == '<' || b == '[')) {
		return 1;
	} else if (a == 'e' /* && b == '€' */) {
		return 1;
	} else if (a == 's' && b == '$') {
		return 1;
	} else if (a == 'l' && b == 'i') {
		return 1;
	} else {
		return 0;
	}
}

static int getEmulatedSize(char c, char a, char b) {
	if (a == b) {
		return 1;
	}
	if (a >= 'a' && a <= 'm') {
		if (a == 'a') {
			if (b != '4' && b != '@' && b != '^') {
				if (b == '/' && c == '\\') {
					return 2;
				}
				return 0;
			}
			return 1;
		}
		if (a == 'b') {
			if (b != '6' && b != '8') {
				if (b == '1' && c == '3') {
					return 2;
				}
				return 0;
			}
			return 1;
		}
		if (a == 'c') {
			if (b != '(' && b != '<' && b != '{' && b != '[') {
				return 0;
			}
			return 1;
		}
		if (a == 'd') {
			if (b == '[' && c == ')') {
				return 2;
			}
			return 0;
		}
		if (a == 'e') {
			if (b != '3' /* && b != '€' */) {
				return 0;
			}
			return 1;
		}
		if (a == 'f') {
			if (b == 'p' && c == 'h') {
				return 2;
			}
			// if (b == '£') {
			// 	return 1;
			// }
			return 0;
		}
		if (a == 'g') {
			if (b != '9' && b != '6') {
				return 0;
			}
			return 1;
		}
		if (a == 'h') {
			if (b == '#') {
				return 1;
			}
			return 0;
		}
		if (a == 'i') {
			if (b != 'y' && b != 'l' && b != 'j' && b != '1' && b != '!' && b != ':' && b != ';' && b != '|') {
				return 0;
			}
			return 1;
		}
		if (a == 'j') {
			return 0;
		}
		if (a == 'k') {
			return 0;
		}
		if (a == 'l') {
			if (b != '1' && b != '|' && b != 'i') {
				return 0;
			}
			return 1;
		}
		if (a == 'm') {
			return 0;
		}
	}
	if (a >= 'n' && a <= 'z') {
		if (a == 'n') {
			return 0;
		}
		if (a == 'o') {
			if (b != '0' && b != '*') {
				if ((b != '(' || c != ')') && (b != '[' || c != ']') && (b != '{' || c != '}') && (b != '<' || c != '>')) {
					return 0;
				}
				return 2;
			}
			return 1;
		}
		if (a == 'p') {
			return 0;
		}
		if (a == 'q') {
			return 0;
		}
		if (a == 'r') {
			return 0;
		}
		if (a == 's') {
			if (b != '5' && b != 'z' && b != '$' && b != '2') {
				return 0;
			}
			return 1;
		}
		if (a == 't') {
			if (b != '7' && b != '+') {
				return 0;
			}
			return 1;
		}
		if (a == 'u') {
			if (b == 'v') {
				return 1;
			}
			if ((b == '\\' && c == '/') || (b == '\\' && c == '|') || (b == '|' && c == '/')) {
				return 2;
			}
			return 0;
		}
		if (a == 'v') {
			if ((b != '\\' || c != '/') && (b != '\\' || c != '|') && (b != '|' || c != '/')) {
				return 0;
			}
			return 2;
		}
		if (a == 'w') {
			if (b == 'v' && c == 'v') {
				return 2;
			}
			return 0;
		}
		if (a == 'x') {
			if ((b != ')' || c != '(') && (b != '}' || c != '{') && (b != ']' || c != '[') && (b != '>' || c != '<')) {
				return 0;
			}
			return 2;
		}
		if (a == 'y') {
			return 0;
		}
		if (a == 'z') {
			return 0;
		}
	}
	if (a >= '0' && a <= '9') {
		if (a == '0') {
			if (b == 'o' || b == 'O') {
				return 1;
			} else if ((b != '(' || c != ')') && (b != '{' || c != '}') && (b != '[' || c != ']')) {
				return 0;
			} else {
				return 2;
			}
		} else if (a == '1') {
			return b == 'l' ? 1 : 0;
		} else {
			return 0;
		}
	} else if (a == ',') {
		return b == '.' ? 1 : 0;
	} else if (a == '.') {
		return b == ',' ? 1 : 0;
	} else if (a == '!') {
		return b == 'i' ? 1 : 0;
	} else {
		return 0;
	}
}

static int8_t getIndex(char c) {
	if (c >= 'a' && c <= 'z') {
		return (int8_t) (c + 1 - 'a');
	} else if (c == '\'') {
		return 28;
	} else if (c >= '0' && c <= '9') {
		return (int8_t) (c + 29 - '0');
	} else {
		return 27;
	}
}

static void filterFragments(char *chars) {
	// bool compare = false;
	int end = 0;
	int count = 0;
	int start = 0;
	while (true) {
		do {
			int index;
			if ((index = indexOfNumber(chars, end)) == -1) {
				return;
			}
			bool foundLowercase = false;
			for (int i = end; i >= 0 && i < index && !foundLowercase; i++) {
				if (isalnum(chars[i]) && !isLowerCaseAlpha(chars[i])) {
					foundLowercase = true;
				}
			}
			if (foundLowercase) {
				count = 0;
			}
			if (count == 0) {
				start = index;
			}
			end = indexOfNonNumber(index, chars);
			int value = 0;
			for (int i = index; i < end; i++) {
				value = value * 10 + chars[i] - 48;
			}
			if (value <= 255 && end - index <= 8) {
				count++;
			} else {
				count = 0;
			}
		} while (count != 4);
		for (int i = start; i < end; i++) {
			chars[i] = '*';
		}
		count = 0;
	}
}

static int indexOfNumber(char *input, int off) {
	size_t input_len = strlen(input);
	for (int i = off; i < (int)input_len && i >= 0; i++) {
		if (input[i] >= '0' && input[i] <= '9') {
			return i;
		}
	}
	return -1;
}

static int indexOfNonNumber(int off, char *input) {
	size_t input_len = strlen(input);
	int i = off;
	while (true) {
		if (i < (int)input_len && i >= 0) {
			if (input[i] >= '0' && input[i] <= '9') {
				i++;
				continue;
			}
			return i;
		}
		return input_len;
	}
}

static bool isLowerCaseAlpha(char c) {
	if (c >= 'a' && c <= 'z') {
		return c == 'v' || c == 'x' || c == 'j' || c == 'q' || c == 'z';
	} else {
		return true;
	}
}

static bool isBadFragment(char *input) {
	size_t input_len = strlen(input);
	bool skip = true;
	for (size_t i = 0; i < input_len; i++) {
		if (!isdigit(input[i]) && input[i] != '\0') {
			skip = false;
			break;
		}
	}
	if (skip) {
		return true;
	}
	int i = firstFragmentId(input);
	int start = 0;
	int end;
	end = fragments_count - 1;
	if (i == _WordFilter.fragments[start] || i == _WordFilter.fragments[end]) {
		return true;
	}
	do {
		int middle = (start + end) / 2;
		if (i == _WordFilter.fragments[middle]) {
			return true;
		}
		if (i < _WordFilter.fragments[middle]) {
			end = middle;
		} else {
			start = middle;
		}
	} while (start != end && start + 1 != end);
	return false;
}

static int firstFragmentId(char *chars) {
	size_t chars_len = strlen(chars);
	if (chars_len > 6) {
		return 0;
	}
	int value = 0;
	for (size_t i = 0; i < chars_len; i++) {
		char c = chars[chars_len - i - 1];
		if (c >= 'a' && c <= 'z') {
			value = value * 38 + c + 1 - 'a';
		} else if (c == '\'') {
			value = value * 38 + 27;
		} else if (c >= '0' && c <= '9') {
			value = value * 38 + c + 28 - '0';
		} else if (c != '\0') {
			return 0;
		}
	}
	return value;
}
