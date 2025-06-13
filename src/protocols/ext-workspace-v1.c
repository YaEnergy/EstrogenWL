#include "protocols/ext-workspace-v1.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wayland-server.h>
#include <wayland-util.h>

#include <wlr/types/wlr_output.h>

#include "util/wl_macros.h"

#include "protocols/transactions.h"

#include "ext-workspace-v1-protocol.h"

//REF 1: https://wayland-book.com/registry/server-side.html
//REF 2: https://wayland.app/protocols/ext-workspace-v1
//REF 3: labwc

enum manager_op_type
{
    MANAGER_GROUP_CREATE_WORKSPACE, //struct group_create_workspace_event*

    MANAGER_WORKSPACE_ACTIVATE,
    MANAGER_WORKSPACE_DEACTIVATE,
    MANAGER_WORKSPACE_ASSIGN, //struct workspace_assign_event*
    MANAGER_WORKSPACE_REMOVE,
};

// Duplicates given null-terminated string.
// Returned string must be freed, returns NULL on fail.
static char* e_strdup(const char* string)
{
    size_t len = strlen(string);
    char* copy = calloc(len + 1, sizeof(*copy));

    if (copy == NULL)
        return NULL;

    strncpy(copy, string, len);
    copy[len] = '\0'; //null-terminator
    
    return copy;
}

static void wl_array_append_uint32_t(struct wl_array* array, uint32_t num)
{
    uint32_t* start = wl_array_add(array, sizeof(uint32_t));

    if (start != NULL)
        *start = num;
}

/* workspace manager schedule */

static void e_ext_workspace_manager_schedule_done_event(struct e_ext_workspace_manager* manager);

/* workspace interface */

/* workspace */

/* workspace group interface */

/* workspace group */

/* workspace manager interface */

/* workspace manager */