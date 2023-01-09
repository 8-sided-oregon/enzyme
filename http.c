#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "http.h"
#include "config.h"
#include "util.h"
#include "log.h"
#include "mime.h"

#define MIN_REQ_SIZE 128
#define MAX_REQ_SIZE 16384
#define TCP_PROTOCOL 6
#define SERVER_STR "Enzyme 0.1"

extern int errno;

static const char *day_mappings[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static const char *month_mappings[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

ret_codes_t parse_request(char *recieved, struct request *req) {
    char method[5], version[9];
    char *tmp_recv, *str1, *name, *content;
    int i, len, status;

    /* Parses the request */
    tmp_recv = recieved;
    str1 = strrep(&tmp_recv, "\r\n");
    for (i = 0;; i++) {
        if (str1 == NULL) {
            o_log(L_ERROR, "bad request");
            return -1;
        }
        
        if (strcmp(str1, "") == 0)
            break;

        if (i == 0) {
            status = sscanf(str1, "%4s %128s %8s", method, req->resource, version);
            if (status != 3)
                return -1;
        }
        else {
            status = process_header_kv(str1, &name, &content);
            if (status < 0) {
                o_log(L_ERROR, "improperly formatted header");
                return -1;
            }

            len = strlen(name);
            for (i = 0; i < len; i++)
                if (name[i] > '@' && name[i] < '[') name[i] += 0x20;

            if (memcmp(name, "user-agent", len) == 0)
                req->user_agent = content;
            else if (memcmp(name, "accept-language", len) == 0)
                req->accept_language = content;
            else if (memcmp(name, "accept-encoding", len) == 0)
                req->accept_encoding = content;

	    free(content);
            free(name);
        }

        str1 = strrep(&tmp_recv, "\r\n");
    }
    
    if (strcmp(method, "GET") != 0)
        return R_METHOD_NOT_SUPPORTED;
    if (strcmp(version, "HTTP/1.1") != 0)
        return R_BAD_REQUEST;

    return R_OKAY;
}

ret_codes_t open_resource(struct connection *conn) {
    struct stat resource_stat;
    int status;

    /* Checks & opens the file */
    status = stat(conn->req.resource, &resource_stat);
    if (status < 0 && (errno == EACCES || errno == ENOENT))
        return R_NOT_FOUND;
    else if (status < 0) {
        o_plog("stat error");
        return R_SERVER_ERROR;
    }
    else if (!S_ISREG(resource_stat.st_mode))
        return R_NOT_FOUND;

    conn->resp.timestamp = resource_stat.st_mtim.tv_sec;
    conn->resp.file_size = resource_stat.st_size;

    conn->resp.mime = get_mime(conn->req.resource);
    conn->resp.fd = open(conn->req.resource, O_RDONLY);

    if (conn->resp.fd < 0) {
        o_plog("open error");
        return R_SERVER_ERROR;
    }

    return R_OKAY;
}

void write_header(int client, char *name, char *content) {
    write(client, name, strlen(name));
    write(client, content, strlen(content));
    write(client, "\r\n", 2);
}

int send_resp_header(int client, struct response *resp) {
    struct tm tstamp_tm, *ptr;
    char buf[193];

    snprintf(buf, 192, "HTTP/1.1 %s\r\n", ret_code_mappings[resp->ret_code]);
    write(client, buf, strlen(buf));

    if (resp->timestamp != 0) {
        ptr = gmtime_r(&resp->timestamp, &tstamp_tm);
        if (ptr == NULL) {
            o_plog("gmtime failed");
            return -1;
        }

        snprintf(buf, 192, "%s, %d %s %d %02d:%02d:%02d GMT",
                day_mappings[tstamp_tm.tm_wday], tstamp_tm.tm_mday, 
                month_mappings[tstamp_tm.tm_mon], tstamp_tm.tm_year + 1900, 
                tstamp_tm.tm_hour, tstamp_tm.tm_min, tstamp_tm.tm_sec);

        write_header(client, "Last-Modified: ", buf);
    }
    if (resp->language != NULL)
        write_header(client, "Language: ", resp->language);

    write_header(client, "Content-Type: ", resp->mime);
    write(client, "\r\n", 2);

    return 0;
}

int rewrite_resource(char path[129]) {
    char *r_src, *r_dst, *begin, tmp_buf[129];
    int i, len;

    for (i = 0; rewrite_rules[i].src != NULL; i++) {
        r_src = rewrite_rules[i].src;
        r_dst = rewrite_rules[i].dst;

        len = strlen(r_src);
        if (r_src[len-1] != '/' || len == 1) {
            o_log(L_DEBUG, "src (%s) is a file", r_src);
            if (strcmp(path, r_src) == 0) {
                strcpy(path, r_dst);
                return 1;
            }
        }
        else {
            o_log(L_DEBUG, "src (%s) is a directory", r_src);
            begin = strbgn(path, r_src);
            if (begin == NULL)
                continue;

            if (*begin == '/')
                begin++;

            len = strlen(r_dst) + strlen(begin) - 1;
            if (len > 129)
                return -1;

            strcpy(tmp_buf, r_dst);
            strcat(tmp_buf, begin);
            strcpy(path, tmp_buf);

            return 1;
        }
    }

    return 0;
}

int escape_path(char *path) {
    char *saveptr, *str, *escaped;
    bool prev, flag;
    int i, j, rc;

    escaped = xmalloc(strlen(path));

    escaped[0] = '\0';
    rc = 0;

    flag = path[strlen(path) - 1] == '/';

    str = strtok_r(path, "/", &saveptr);
    for (;;) {
        if (str == NULL)
            break;

        if (strcmp(str, "..") != 0) {
            strcat(escaped, "/");
            strcat(escaped, str);
        }
        else {
            rc += 1;
        }

        str = strtok_r(NULL, "/", &saveptr);
    }

    if (flag)
        strcat(escaped, "/");

    prev = true;
    for (i = 0, j = 0; escaped[i] != '\0'; i++) {
        if (!(escaped[i] == '/' && prev)) {
            path[j++] = escaped[i];
        }
        prev = path[i] == '/';
    }
    path[j] = '\0';

    free(escaped);

    return rc;
}

char *recv_request(int client) {
    char c, *recved, *end;
    int i, buf_size, data_cnt, new_size, zero_cntr;

    recved = xmalloc(MIN_REQ_SIZE + 1);

    for (buf_size = 0, zero_cntr = 0, i = 0;; i++){
        data_cnt = read(client, recved + buf_size, MIN_REQ_SIZE);
        
        if (data_cnt < 0) {
            o_plog("recv failed");
            exit(1);
        }

        buf_size += data_cnt;
        o_log(L_DEBUG, "data_cnt: %d", data_cnt);
        end = memsrch(recved, buf_size, "\r\n\r\n", 4);

        if (end != NULL)
            break;

        if (data_cnt != 0) {
            zero_cntr = 0;
            
            new_size = buf_size + MIN_REQ_SIZE;
            if (new_size > MAX_REQ_SIZE) {
                free(recved);
                return NULL;
            }
            recved = xrealloc(recved, new_size + 1);
        }
        /* horrible hack */
        else if (++zero_cntr == 10)
                return NULL;
    }

    recved[buf_size] = '\0';

    for (i = 0; i < buf_size; i++) {
        c = recved[i];
        if ((c < ' ' || c > '~') && (c < '\b' || c > '\r')) {
            free(recved);
            return NULL;
        }
    }

    return recved;
}

int process_header_kv(char *header, char **name, char **content) {
    int n_len, c_len;
    char *pos;

    pos = strstr(header, ": ");
    if (pos == NULL)
        return -1;

    n_len = pos - header;
    *name = xmalloc(n_len);
    memcpy(*name, header, n_len - 1);
    (*name)[n_len - 1] = '\0';

    c_len = strlen(pos + 2);
    *content = xmalloc(c_len + 1);
    memcpy(*content, pos + 2, c_len);
    (*content)[n_len] = '\0';

    return 0;
}
