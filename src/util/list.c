#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

#include "util/list.h"

// Inits a list for (capacity) items, starting capacity must be larger than 0.
// Returns true on success, false on fail.
bool e_list_init(struct e_list* list, int capacity)
{
    assert(list && capacity > 0);

    list->count = 0;
    list->capacity = capacity;
    list->items = calloc(capacity, sizeof(void*));

    if (list->items == NULL)
    {
        free(list);
        return false;
    }

    return true;
}

void* e_list_at(struct e_list* list, int i)
{
    assert(list);

    if (i < 0 || i >= list->count)
        return NULL;

    return list->items[i];
}

// Returns true on success, false on fail.
static bool e_list_ensure_capacity(struct e_list* list, int capacity)
{
    assert(list && list->capacity > 0);

    if (list->capacity >= capacity)
        return true;

    int new_capacity = list->capacity;

    while (new_capacity < capacity)
        new_capacity <<= 1;

    void** new_items = realloc(list->items, sizeof(*list->items) * new_capacity);

    if (new_items == NULL)
        return false;

    //fill new spots with NULL
    for (int i = list->capacity; i < new_capacity; i++)
        new_items[i] = NULL;

    list->items = new_items;
    list->capacity = new_capacity;

    return true;
}

bool e_list_add(struct e_list* list, void* item)
{
    assert(list && item);    

    e_list_insert(list, item, list->count);

    return true;
}

bool e_list_insert(struct e_list* list, void* item, int index)
{
    assert(list && item);

    if (index < 0 || index > list->count)
        return false;

    if (!e_list_ensure_capacity(list, list->count + 1))
        return false;

    //shift items to the right by 1 space
    for (int i = list->count - 1; i >= index; i--)
        list->items[i + 1] = list->items[i];

    list->items[index] = item;
    list->count++;

    return true;
}

// Returns the index of the first occurence of item in the list.
// Returns -1 if not found.
int e_list_find_index(struct e_list* list, void* item)
{
    assert(list && item);

    for (int i = 0; i < list->count; i++)
    {
        if (list->items[i] == item)
            return i;
    }

    return -1;
}

bool e_list_remove(struct e_list* list, void* item)
{
    assert(list && item);

    for (int i = 0; i < list->count; i++)
    {
        if (list->items[i] == item)
            return e_list_remove_index(list, i);
    }

    return false;
}

bool e_list_remove_index(struct e_list* list, int index)
{
    assert(list);

    if (index < 0 || index >= list->count)
        return false;

    //Shift items in front of index to the left by 1 space, overwriting item at index
    for (int i = index + 1; i < list->count; i++)
        list->items[i - 1] = list->items[i];

    //set last item (count - 1) to NULL, as it's now at (count - 2) [before decrement count]
    list->items[list->count - 1] = NULL;
    list->count--;

    return true;
}

// Remove all items in the list.
void e_list_clear(struct e_list* list)
{
    assert(list);

    for (int i = 0; i < list->count; i++)
        list->items[i] = NULL;

    list->count = 0;
}

// Swaps 2 indexes in 2 separate lists.
// List a & b are allowed to be the same list.
// Returns true on success, false on fail.
bool e_list_swap_outside(struct e_list* list_a, int index_a, struct e_list* list_b, int index_b)
{
    assert(list_a && list_b);

    if (index_a < 0 || index_a >= list_a->count)
        return false;
    
    if (index_b < 0 || index_b >= list_b->count)
        return false;

    void* tmp = list_a->items[index_a];
    list_a->items[index_a] = list_b->items[index_b];
    list_b->items[index_b] = tmp;

    return true;
}

void e_list_fini(struct e_list* list)
{
    assert(list);

    free(list->items);
}
