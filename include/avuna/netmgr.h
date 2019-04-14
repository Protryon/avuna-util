//
// Created by p on 4/14/19.
//

#ifndef AVUNA_UTIL_NETMGR_H
#define AVUNA_UTIL_NETMGR_H

#include <avuna/tls.h>
#include <avuna/buffer.h>
#include <avuna/pmem.h>

struct netmgr_connection {
    struct mempool* pool;
    int fd;
    int tls;
    int tls_handshaked;
    SSL* tls_session;
    struct buffer read_buffer;
    struct buffer write_buffer;
    int write_available;
    int safe_close;
    int (*read)(struct netmgr_connection* conn, uint8_t* read_buf, size_t read_buf_len);
    void (*on_closed)(struct netmgr_connection* conn);
    struct netmgr_thread* controlling_thread;
    void* extra;
};

struct netmgr_thread {
    struct mempool* pool;
    void (*pre_poll)(struct netmgr_thread* thread);
    void* extra;
    int _epoll_fd;
};

void netmgr_trigger_write(struct netmgr_connection* conn);

struct netmgr_thread* netmgr_create();

int netmgr_run(struct netmgr_thread* netmgr);

int netmgr_add_connection(struct netmgr_thread* netmgr, struct netmgr_connection* conn);

#endif //AVUNA_UTIL_NETMGR_H
