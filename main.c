#define _GNU_SOURCE

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <poll.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/sendfile.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "config.h"
#include "log.h"
#include "http.h"
#include "util.h"
#include "queue.h"

#define TCP_PROTOCOL 6
#define CONN_SEND_MAX 65536

char *prg_name;
bool color_enabled;
bool debug_sleeps;

extern int errno;

void print_help(int code) {
    fprintf(stderr, "Usage: %s [bind addr] [port]\n", prg_name);
    exit(code);
}

ret_codes_t handle_conn_start(struct connection *conn) {
    char *recieved;
    int status;

    /* Recieves & parses the HTTP request */
    recieved = recv_request(conn->client_fd);
    if (recieved == NULL)
        return R_BAD_REQUEST;

    status = parse_request(recieved, &conn->req);
    if (status != R_OKAY)
        return status;
    free(recieved);

    conn->state = BEGIN;

    return R_OKAY;
}

ret_codes_t handle_get(struct connection *conn) {
    int rc, val, status;

    /* Rewrites & escapes the resource */
    o_log(L_DEBUG, "resource: %s", conn->req.resource);
    do {
        rc = rewrite_resource(conn->req.resource);
        if (rc < 0)
            return R_SERVER_ERROR;
    } while (rc != 0);

    escape_path(conn->req.resource);
    o_log(L_DEBUG, "rewritten: /%s", conn->req.resource);

    status = open_resource(conn);
    if (status != R_OKAY)
        return status;

    if (conn->resp.dirlist_path == NULL) {
        val = 1;
        status = setsockopt(conn->client_fd, TCP_PROTOCOL, TCP_CORK, &val, sizeof(int));
        if (status < 0) {
            o_plog("setsockopt failed");
            exit(1);
        }

        send_resp_header(conn->client_fd, &conn->resp);
        conn->state = SENT_HEADER;

        val = 0;
        status = setsockopt(conn->client_fd, TCP_PROTOCOL, TCP_CORK, &val, sizeof(int));
        if (status < 0) {
            o_plog("setsockopt failed");
            exit(1);
        }
    } 
    else {
        o_log(L_ERROR, "Directory listing not implemeted yet.");
        return R_SERVER_ERROR;
    }

    return R_OKAY;
}

ret_codes_t handle_post(struct connection *conn) {
    return R_METHOD_NOT_SUPPORTED;
}

ret_codes_t handle_resp_send(struct connection *conn) {
    int status;

    conn->state = SENDING_BODY;

#ifndef __ignore_debug_
    if (debug_sleeps)
        sleep(1);
#endif

    status = sendfile(conn->client_fd, conn->resp.fd, 
            &conn->resp.sent_bytes, CONN_SEND_MAX);

    if (status < 0 && errno != EPIPE) {
        o_plog("sendfile failed");
    }

    o_log(L_DEBUG, "%d fd %d sent", conn->resp.fd, status);

    if (status != CONN_SEND_MAX) {
        if (conn->resp.fd > 0)
            close(conn->resp.fd);
        close(conn->client_fd);
        conn->state = DONE;
    }

    return R_OKAY;
}

int main(int argc, char **argv) {
    int status, port, server, client, len1, val;
    bool conn_flag;
    char client_addr_buf[NI_MAXHOST];
    const char *ret_html;
    ret_codes_t http_status;

    struct response err_resp;
    struct connection *cur_conn, *p_conn;
    struct sockaddr client_addr;
    struct pollfd server_pollfd, client_pollfd;
    struct addrinfo hints, *res, *rp;

#ifndef __ignore_debug_
    debug_sleeps = secure_getenv("DEBUG_SLEEPS") != NULL;
    fprintf(stderr, "debug sleeps: %p\n", secure_getenv("DEBUG_SLEEPS"));
#endif
    color_enabled = true;
    conn_flag = true;
    
    prg_name = argv[0];

    if (argc < 3)
        print_help(1);

    /* Parses the address from the first argument */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    status = getaddrinfo(argv[1], argv[2], &hints, &res);
    if (status != 0)
        print_help(1);

    /* Parses the port from the second argument */
    status = sscanf(argv[2], "%d", &port);
    if (status != 1)
        print_help(1);

    /* Creates & binds the server socket */
    for(rp = res; rp != NULL; rp = rp->ai_next) {
        server = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

        if (server == -1) {
            close(server);
            continue;
        }

        status = bind(server, rp->ai_addr, rp->ai_addrlen);
        if (status == 0)
            break;

        close(server);
    }

    freeaddrinfo(res);

    if (status < 0) {
        perror("bruh");
        print_help(1);
    }

    listen(server, 16);
    o_log(L_INFO, "Started listening on %s:%s", argv[1], argv[2]);

    server_pollfd.fd = server;
    server_pollfd.events = POLLIN;

    init_queue();

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        o_plog("signal failed");
        exit(1);
    }

    /* Main loop */
    for(;;) {
        len1 = sizeof(client_addr);

        if (conn_flag) {
            o_log(L_DEBUG, "conn_flag was set.");

            client = accept(server, &client_addr, (socklen_t *)&len1);
        }
        else {
            server_pollfd.revents = 0;
            client = -1;

            o_log(L_DEBUG, "Polling for connections.");

            status = poll(&server_pollfd, 1, 0);
            if (status < 0) {
                o_plog("poll failed");
                exit(1);
            }

            if (server_pollfd.revents & POLLIN) {
                client = accept(server, &client_addr, (socklen_t *)&len1);
                o_log(L_DEBUG, "Accepted connection after polling.");
            } else {
                o_log(L_DEBUG, "Didn't find any.");
            }
        }

        conn_flag = false;

        if (client < 0) {
            cur_conn = next_conn(cur_conn);
            o_log(L_DEBUG, "Switching to cur_conn: %p", cur_conn);
            if (cur_conn == NULL) {
                o_log(L_DEBUG, "cur_conn was nil, setting conn_flag...");
                conn_flag = true;
                continue;
            }
        }
        else {
            status = getnameinfo(&client_addr, (socklen_t)len1, client_addr_buf, 
                    sizeof(client_addr_buf), NULL, 0, NI_NUMERICHOST);
            if (status < 0) {
                o_plog("fuck");
                exit(1);
            }

            o_log(L_INFO, "Accepted connection from %s", client_addr_buf);

            cur_conn = xmalloc(sizeof(struct connection));
            memset(cur_conn, 0, sizeof(struct connection));
            cur_conn->client_fd = client;

            o_log(L_DEBUG, "%p: Created node", cur_conn);

            push_queue(cur_conn, HEAP_ALLOCATED);
            http_status = handle_conn_start(cur_conn);

            if (http_status != R_OKAY)
                goto error;
        }

        switch (cur_conn->state) {
        case BEGIN:
            o_log(L_DEBUG, "%p: Found connection in BEGIN state.", cur_conn);
            if (cur_conn->req.method == M_GET)
                http_status = handle_get(cur_conn);
            else {
                http_status = R_METHOD_NOT_SUPPORTED;
                goto error;
            }
            break;
        case SENT_HEADER:
            o_log(L_DEBUG, "%p: Found connection in SENT_HEADER state.", cur_conn);
        case SENDING_BODY:
            o_log(L_DEBUG, "%p: Found connection in SENDING_BODY state.", cur_conn);
            http_status = handle_resp_send(cur_conn);
            break;
        case DONE:
            o_log(L_DEBUG, "%p: Found connection in DONE state.", cur_conn);

            p_conn = cur_conn;
            cur_conn = prev_conn(cur_conn);
            remove_node(p_conn);

            o_log(L_DEBUG, "Switched to cur_conn (previous node): %p", cur_conn);

            continue;
        }

error:
        o_log(L_DEBUG, "%p: Checking for errors...", cur_conn);

        if (http_status == R_OKAY)
            continue;

        o_log(L_INFO, "Client returned w/ %d.", http_status);
        
        memset(&err_resp, 0, sizeof(err_resp));
        err_resp.ret_code = http_status;
        err_resp.timestamp = 0;
        err_resp.mime = "text/html";

        val = 1;
        status = setsockopt(cur_conn->client_fd, TCP_PROTOCOL, TCP_CORK, &val, sizeof(int));
        if (status < 0) {
            o_plog("setsockopt failed");
            exit(1);
        }

        send_resp_header(cur_conn->client_fd, &err_resp);

        ret_html = ret_code_mappings[http_status];
        write(cur_conn->client_fd, ret_html, strlen(ret_html));
        write(cur_conn->client_fd, "\n", 1);

        /* Checks if the pipe is broken */
        client_pollfd.fd = cur_conn->client_fd;
        client_pollfd.events = POLLOUT;
        client_pollfd.revents = 0;

        poll(&client_pollfd, 1, 0);

        if (client_pollfd.revents & POLLOUT) {
            val = 0;
            status = setsockopt(cur_conn->client_fd, TCP_PROTOCOL, TCP_CORK, &val, sizeof(int));
            if (status < 0) {
                o_plog("setsockopt failed");
                exit(1);
            }

            o_log(L_DEBUG, "%p: Sent error message %s", cur_conn, ret_html);
        }
        else {
            o_log(L_DEBUG, "%p: Could not sent error message, socket was not writable.", cur_conn);
        }

        close(cur_conn->client_fd);
        remove_node(cur_conn);
    }
}
