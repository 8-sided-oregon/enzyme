#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <execinfo.h>

#include "util.h"

#define BT_BUF_SIZE 128

char *strbgn(const char *s1, const char *s2) {
    int i;
    for (i = 0; s1[i] != '\0' && s2[i] != '\0'; i++) {
        if (s1[i] != s2[i])
            return NULL;
    }
    if (s2[i] == '\0')
        return s1+i;
    return NULL;
}

char *strrep(char **stringp, const char *delim) {
    char *begin, *end;

    begin = *stringp;

    if (*stringp == NULL)
        return NULL;

    end = strstr(*stringp, delim);
    if (end == NULL) {
        *stringp = NULL;
        return NULL;
    }

    *end = '\0';
    *stringp = end + strlen(delim);

    return begin;
}

void print_bt() {
    int n_ptrs;
    void *bt_buf[BT_BUF_SIZE];

    n_ptrs = backtrace(bt_buf, BT_BUF_SIZE);
    fprintf(stderr, "backtrace() provided %d function addresses\n", n_ptrs);
    backtrace_symbols_fd(bt_buf, n_ptrs, STDOUT_FILENO);
}

void *memsrch(void *haystack, size_t h_len, void *needle, size_t n_len) {
    size_t i, j;
    void *begin;

    for (i = 0, j = 0; i < h_len; i++) {
        if (((char *)haystack)[i] == ((char *)needle)[j] && j == 0) 
            begin = haystack + i;
        if (((char *)haystack)[i] == ((char *)needle)[j]) {
            if (++j == n_len)
                return begin;
        }
        else
            j = 0;
    }

    return NULL;
}

void *xmalloc(size_t size) {
    void *ret;

    //print_bt();

    ret = malloc(size);
    if (ret == NULL) {
        perror("malloc failed");
        print_bt();
        exit(1);
    }

    return ret;
}

void *xrealloc(void *ptr, size_t size) {
    void *ret;

    //print_bt();
    
    ret = realloc(ptr, size);
    if (ret == NULL) {
        perror("realloc failed");
        print_bt();
        exit(1);
    }

    return ret;
}
