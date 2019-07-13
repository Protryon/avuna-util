//
// Created by p on 4/14/19.
//

#include <avuna/netmgr.h>
#include <avuna/netmgr.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>

void netmgr_trigger_write(struct netmgr_connection* conn) {
    if (conn->write_available && conn->write_buffer.size > 0) {
        for (struct llist_node* node = conn->write_buffer.buffers->head; node != NULL; ) {
            struct buffer_entry* entry = node->data;
            size_t written;
            if (conn->tls) {
                ssize_t mtr = SSL_write(conn->tls_session, entry->data, (int) entry->size);
                if (mtr < 0) {
                    int ssl_error = SSL_get_error(conn->tls_session, (int) mtr);
                    if (ssl_error == SSL_ERROR_SYSCALL && errno == EAGAIN) {
                        conn->write_available = 0;
                        break;
                    } else if (ssl_error != SSL_ERROR_WANT_WRITE && ssl_error != SSL_ERROR_WANT_READ) {
                        conn->safe_close = 1;
                        return;
                    }
                }
                written = (size_t) mtr;
            } else {
                ssize_t mtr = write(conn->fd, entry->data, entry->size);
                if (mtr < 0 && errno == EAGAIN) {
                    conn->write_available = 0;
                    break;
                } else if (mtr < 0) {
                    conn->safe_close = 1;
                    break;
                }
                written = (size_t) mtr;
            }
            if (written < entry->size) {
                entry->data += written;
                entry->size -= written;
                conn->write_available = 1;
                conn->write_buffer.size -= written;
                break;
            } else {
                conn->write_buffer.size -= written;
                pprefree_strict(conn->write_buffer.pool, entry->data_root);
                struct llist_node* next = node->next;
                llist_del(conn->write_buffer.buffers, node);
                node = next;
                if (node == NULL) {
                    conn->write_available = 1;
                    break;
                } else {
                    continue;
                }
            }
        }
    }
}

struct netmgr_thread* netmgr_create() {
    struct mempool* pool = mempool_new();
    struct netmgr_thread* netmgr = pcalloc(pool, sizeof(struct netmgr_thread));
    netmgr->pool = pool;
    netmgr->_epoll_fd = epoll_create1(0);
    return netmgr;
}

void _netmgr_run_thread(struct netmgr_thread* param);

int netmgr_run(struct netmgr_thread* netmgr) {
    pthread_t pt;
    return pthread_create(&pt, NULL, (void*) _netmgr_run_thread, netmgr);
}

int netmgr_add_connection(struct netmgr_thread* netmgr, struct netmgr_connection* conn) {
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLOUT | EPOLLET;
    event.data.ptr = conn;
    conn->controlling_thread = netmgr;
    if (epoll_ctl(netmgr->_epoll_fd, EPOLL_CTL_ADD, conn->fd, &event)) {
        return 1;
    }
    return 0;
}


#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"

void _netmgr_run_thread(struct netmgr_thread* param) {
    struct epoll_event events[128];
    while (1) {
        if (param->pre_poll != NULL) {
            param->pre_poll(param);
        }
        int epoll_status = epoll_wait(param->_epoll_fd, events, 128, -1);
        if (epoll_status < 0) {
            //epoll error in worker thread
            continue;
        } else if (epoll_status == 0) {
            continue;
        }
        for (int i = 0; i < epoll_status; ++i) {
            struct epoll_event* event = &events[i];
            struct netmgr_connection* conn = event->data.ptr;
            if (conn->safe_close) {
                conn->on_closed(conn);
                continue;
            }
            if (event->events == 0) continue;

            if (event->events & EPOLLHUP) {
                conn->on_closed(conn);
                continue;
            }
            if (event->events & EPOLLERR) {
                conn->on_closed(conn);
                continue;
            }

            if (conn->tls && !conn->tls_handshaked) {
                int r = SSL_accept(conn->tls_session);
                if (r == 1) {
                    conn->tls_handshaked = 1;
                } else if (r == 2) {
                    conn->on_closed(conn);
                    continue;
                } else {
                    int err = SSL_get_error(conn->tls_session, r);
                    if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
                        conn->on_closed(conn);
                        continue;
                    }
                }
                continue;
            }


            if (event->events & EPOLLOUT) {
                conn->write_available = 1;
                netmgr_trigger_write(conn);
            }

            if (event->events & EPOLLIN) {
                void* read_buf = NULL;
                size_t read_total = 0;
                if (conn->tls) {
                    size_t read_capacity = (size_t) SSL_pending(conn->tls_session);
                    if (read_capacity == 0) {
                        ioctl(conn->fd, FIONREAD, &read_capacity);
                        if (read_capacity < 64) {
                            read_capacity = 1024;
                        }
                    }
                    ++read_capacity;
                    read_buf = pmalloc(conn->pool, read_capacity);
                    ssize_t r;
                    while ((r = SSL_read(conn->tls_session, read_buf + read_total, (int) (read_capacity - read_total))) > 0) {
                        read_total += r;
                        if (read_total == read_capacity) {
                            read_capacity *= 2;
                            read_buf = prealloc(conn->pool, read_buf, read_capacity);
                        }
                    }
                    if (r == 0) {
                        conn->on_closed(conn);
                        continue;
                    } else { // < 0
                        int ssl_error = SSL_get_error(conn->tls_session, (int) r);
                        if (!(ssl_error == SSL_ERROR_SYSCALL && errno == EAGAIN) && ssl_error != SSL_ERROR_WANT_WRITE && ssl_error != SSL_ERROR_WANT_READ) {
                            conn->on_closed(conn);
                            continue;
                        }
                    }
                } else {
                    size_t read_capacity = 0;
                    ioctl(conn->fd, FIONREAD, &read_capacity);
                    ++read_capacity;
                    read_buf = pmalloc(conn->pool, read_capacity);
                    ssize_t r;
                    while ((r = read(conn->fd, read_buf + read_total, read_capacity - read_total)) > 0) {
                        read_total += r;
                        if (read_total == read_capacity) {
                            read_capacity *= 2;
                            read_buf = prealloc(conn->pool, read_buf, read_capacity);
                        }
                    }
                    if (r == 0 || (r < 0 && errno != EAGAIN)) {
                        conn->on_closed(conn);
                        continue;
                    }
                }
                int p = conn->read(conn, read_buf, read_total);
                if (p == 1) {
                    conn->on_closed(conn);
                    continue;
                }
            }
        }
    }
}
