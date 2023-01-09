#include <string.h>
#include <stdlib.h>

#include "config.h"
#include "mime.h"

char *get_last_sstr(char *s1, const char *s2) {
    char *str, *ret, *saveptr;

    ret = NULL;
    str = strtok_r(s1, s2, &saveptr);
    for (; str != NULL;) {
        ret = str;
        str = strtok_r(NULL, s2, &saveptr);
    }

    return ret;
}

char *get_mime(const char *resource) {
    char *ext, *fname, *copied;
    int i;

    copied = strdup(resource);

    fname = get_last_sstr(copied, "/");
    if (strstr(fname, ".") == NULL) {
        free(copied);
        return "application/octet-stream";
    }
    ext = get_last_sstr(fname, ".");

    for (i = 0; mimes[i].ext != NULL; i++) {
        if (strcmp(mimes[i].ext, ext) == 0) {
            free(copied);
            return mimes[i].mime;
        }
    }

    free(copied);
    return "application/octet-stream";
}
