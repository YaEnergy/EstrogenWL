#pragma once

#include <stdbool.h>

// Just a simple generic list using an array
struct e_list
{
    // items
    void** items;
    
    // amount of items this list is holding
    int count;

    // amount of items this list can hold
    int capacity;
};

// Inits a list for (capacity) items, starting capacity must be larger than 0.
// Returns true on success, false on fail.
bool e_list_init(struct e_list* list, int capacity);

// Returns item at index in the list.
// Returns NULL if index is out of bounds.
void* e_list_at(struct e_list* list, int index);

// Adds an item to the end of the list.
// Returns true on success, false on fail.
bool e_list_add(struct e_list* list, void* item);

// Inserts an item at index into the list.
// Returns true on success, false on fail.
bool e_list_insert(struct e_list* list, void* item, int index);

// Returns the index of the first occurence of item in the list.
// Returns -1 if not found.
int e_list_find_index(struct e_list* list, void* item);

// Removes first occurence of item in the list.
// Returns true on success, false on fail.
bool e_list_remove(struct e_list* list, void* item);

// Removes item at index in the list.
// Returns true on success, false on fail.
bool e_list_remove_index(struct e_list* list, int index);

// Swaps 2 indexes in 2 separate lists.
// List a & b are allowed to be the same list.
// Returns true on success, false on fail.
bool e_list_swap_outside(struct e_list* list_a, int index_a, struct e_list* list_b, int index_b);

// Frees the list, and memory for holding the item pointers, but not the items themselves.
void e_list_fini(struct e_list* list);
