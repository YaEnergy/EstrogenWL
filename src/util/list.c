#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

#include "util/list.h"

// Create a list for (capacity) items, starting capacity must be larger than 0.
// Returns NULL on fail.
struct e_list* e_list_create(int capacity)
{
    assert(capacity > 0);

    struct e_list* list = calloc(1, sizeof(*list));

    if (list == NULL)
        return NULL;

    list->count = 0;
    list->capacity = capacity;
    list->items = calloc(capacity, sizeof(void*));

    if (list->items == NULL)
    {
        free(list);
        return NULL;
    }

    return list;
}

void* e_list_at(struct e_list* list, int i)
{
    if (i < 0 || i >= list->count)
        return NULL;

    return list->items[i];
}

// Returns true on success, false on fail.
static bool e_list_expand(struct e_list* list)
{
    assert(list && list->capacity > 0);

    void* new_items = realloc(list->items, sizeof(*list->items) * list->capacity * 2);

    if (new_items == NULL)
        return false;

    list->items = new_items;
    list->capacity *= 2;

    return true;
}

bool e_list_add(struct e_list* list, void* item)
{
    assert(list && item);    

    if (list->count + 1 > list->capacity)
    {
        if (!e_list_expand(list))
            return false;
    }

    list->items[list->count] = item;
    list->count++;

    return true;
}

bool e_list_insert(struct e_list* list, void* item, int i)
{
    assert(list && item);

    if (i < 0 || i >= list->count)
        return false;

    if (list->count + 1 > list->capacity)
    {
        if (!e_list_expand(list))
            return false;
    }

    // move the items at and after out of the way to make space for this item
    memmove(list->items[i + 1], list->items[i], (list->count - i) * sizeof(void*));

    list->items[i] = item;
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

bool e_list_remove_index(struct e_list* list, int i)
{
    assert(list);

    if (i < 0 || i >= list->count)
        return false;

    list->items[i] = NULL;
    list->count--;
    
    // Shift items in front of the removed item back
    if (list->count != i)
        memmove(list->items[i], list->items[i + 1], (list->count - i) * sizeof(void*));

    return true;
}

void e_list_destroy(struct e_list* list)
{
    assert(list);

    free(list->items);

    free(list);
}