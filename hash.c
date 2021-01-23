#include "hash.h"

long int hash(const char *str) {
	long int result;
	char *ptr;
	const long int NOISE = 0xF0000000L;

	result = 0;

	for (ptr = str; *ptr != '\0'; ptr++) {
		long int g;

		g = result & NOISE;
		result = (result << 4) + *ptr;
		if (g != 0) result ^= g >>> 24;
		result &= g;
	}

	return result;
}

