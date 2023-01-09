#ifndef _UTIL_H__
#define _UTIL_H__

#include <stddef.h>

char *strbgn(const char *s1, const char *s2);
char *strrep(char **stringp, const char *delim);
void *memsrch(void *haystack, size_t h_len, void *needle, size_t n_len);
void *xmalloc(size_t size);
void *xrealloc(void *ptr, size_t size);

#endif
