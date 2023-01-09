#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "util.h"
#include "http.h"
#include "queue.h"

struct queue_node {
    struct queue_node *next;
    struct queue_node *prev;

    struct connection *conn;

    int flags;
} http_queue;

void init_queue(void) {
    memset(&http_queue, 0, sizeof(http_queue));
}

void push_queue(struct connection *conn, int flags) {
    struct queue_node *n, *p;
    n = &http_queue;
    while (n->next != NULL) { n = n->next; }

    n->next = xmalloc(sizeof(struct queue_node));
    p = n;
    n = n->next;
    n->next = NULL;
    n->prev = p;
    n->conn = conn;
    n->flags = flags;
}

int remove_node(struct connection *conn) {
    struct queue_node *n, *p;

    if (conn == NULL)
        return -1;

    n = &http_queue;
    while ((n = n->next) != NULL && n->conn != conn) {}

    if (n == NULL)
        return -1;

    if (n->flags & HEAP_ALLOCATED)
        free(n->conn);

    o_log(L_DEBUG, "conn: %p, n: %p, n->conn: %p, n->next: %p, n->prev: %p", conn, n, n->conn, n->next, n->prev);

    p = n->next;
    n = n->prev;

    free(n->next);
    n->next = NULL;

    if (p != NULL) {
        n->next = p;
        p->prev = n;
    }

    return 0;
}

struct connection *next_conn(struct connection *conn) {
    struct queue_node *n;

    n = &http_queue;
    while ((n = n->next) != NULL && n->conn != conn) {}

    if (n == NULL)
        return NULL;
    
    if (n->next == NULL)
        return http_queue.next->conn;

    return n->next->conn;
}

struct connection *prev_conn(struct connection *conn) {
    struct queue_node *n;

    n = &http_queue;
    while ((n = n->next) != NULL && n->conn != conn) {}

    if (n == NULL)
        return http_queue.conn;
    
    if (n->prev == NULL)
        return http_queue.conn;

    return n->prev->conn;
}
