#ifndef _QUEUE_H__
#define _QUEUE_H__

#define HEAP_ALLOCATED 1

void init_queue(void);
void push_queue(struct connection *conn, int flags);
int remove_node(struct connection *conn);
struct connection *next_conn(struct connection *conn);
struct connection *prev_conn(struct connection *conn);

#endif
