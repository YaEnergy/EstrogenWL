#pragma once

#include <wayland-server-core.h>
#include <wayland-util.h>

// Some useful wl macros

#define SIGNAL_CONNECT(signal, listener, notify_func) listener.notify = notify_func; wl_signal_add(&signal, &listener);

// More clear name
#define SIGNAL_DISCONNECT(listener) wl_list_remove(&listener.link);

// Append to the end of wl linked list (list is head)
#define wl_list_append(list, elm) wl_list_insert(list.prev, elm);
