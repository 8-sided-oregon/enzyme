#ifndef _CONFIG_H__
#define _CONGIH_H__

/* User configurable variables */

static const struct {
    char *ext;
    char *mime;
} mimes[] = {
    /* file extension       mime string */
    { "html",               "text/html"             },
    { "css"                 "text/css"              },
    { "js",                 "text/javascript"       },
    { "jpg",                "image/jpeg"            },
    { "jpeg",               "image/jpeg"            },
    { "jfif",               "image/jpeg"            },
    { "gif",                "image/gif"             },
    { "png",                "image/png"             },
    { "webp"                "image/webp"            },
    { "txt",                "text/plain"            },
    { "c",                  "text/plain"            },
    { "h",                  "text/plain"            },
    { "rs",                 "text/plain"            },
    { "conf",               "text/plain"            },
    { "wasm",               "application/wasm"      },
    { "zip",                "application/zip"       },
    { "xz",                 "application/x-xz"      },
    { "gz",                 "application/gzip"      },
    { "tar",                "application/x-tar"     },
    { "json",               "application/json"      },
    { "xml",                "application/xml"       },
    { NULL,                 NULL                    },
};

/* Reads a file a path other than the specified. Note that when directing to a
 * directory, you must put a forward slash at the end of both the src and dest
 * paths. Also note that this cannot and will not redirect someone bellow the
 * directory the server was started in. If you need to do that, use bind mounts.
 *
 * DANGER: This feature IS vulnerable to timing attacks. DO NOT put any
 *         sensitive info in here. Though tbh, I don't know why you would be
 *         trusting this server with sensitive info anyways.
 */

static const struct {
    char *src;
    char *dst;
} rewrite_rules[] = {
    /* source               destination */
    { "/",                  "/index.html"           },
    { "/build/",            "/"                     },

    /*
    { "/downloads/",        "/dls/"                 },
    { "/home/",             "/bob/"                 },
    */

    { NULL,                 NULL,                   },
};

#endif
