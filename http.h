#ifndef _HTTP_H__
#define _HTTP_H__

#include <time.h>
#include <sys/types.h>

typedef enum methods {
    M_GET,
    M_POST,
    M_PUT,
    M_DEL,
} methods_t;

typedef enum ret_codes {
    R_OKAY,
    R_BAD_REQUEST,
    R_NOT_FOUND,
    R_SERVER_ERROR,
    R_METHOD_NOT_SUPPORTED,
    R_VERSION_NOT_SUPPORTED,
} ret_codes_t;

struct request {
    char resource[129];
    methods_t method;

    char *accept_language;
    char *accept_encoding;
    char *user_agent;
};

struct response {
    int fd;
    char *dirlist_path;

    off_t file_size;
    off_t sent_bytes;

    methods_t method;
    ret_codes_t ret_code;
    time_t timestamp;

    char *mime;
    char *language;
};

struct connection {
    int client_fd;

    struct request req;
    struct response resp;

    enum queue_state {
        BEGIN,
        SENT_HEADER,
        SENDING_BODY,
        DONE,
    } state;
};

static const char *ret_code_mappings[] = {
    /*[R_OKAY]                     =*/ "200 OK",
    /*[R_BAD_REQUEST]              =*/ "400 Bad Request",
    /*[R_NOT_FOUND]                =*/ "404 Not Found",
    /*[R_SERVER_ERROR]             =*/ "500 Internal Server Error",
    /*[R_METHOD_NOT_SUPPORTED]     =*/ "501 Not Implemented",
    /*[R_VERSION_NOT_SUPPORTED]    =*/ "505 HTTP Version Not Supported",
};

static const char *method_mappings[] = {
    "GET",
    "POST",
    "PUT",
    "DEL",
};

char *recv_request(int client);
int escape_path(char *path);
int process_header_kv(char *header, char **name, char **content);
int send_resp_header(int client, struct response *resp);
int rewrite_resource(char path[129]);
int send_resp_header(int client, struct response *resp);
ret_codes_t parse_request(char *recieved, struct request *req);
ret_codes_t open_resource(struct connection *conn);

#endif
