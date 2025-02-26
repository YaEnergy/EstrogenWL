#pragma once

#include <stdbool.h>

// Just a simple list using an array
// All items must be of the same type
struct e_list
{
    // items
    void** items;
    
    // amount of items this list is holding
    int count;

    // amount of items this list can hold
    int capacity;
};

// Create a list for (capacity) items, starting capacity must be larger than 0.
// Returns NULL on fail.
struct e_list* e_list_create(int capacity);

// Returns item at index i in the list.
// Returns NULL if index i is out of bounds.
void* e_list_at(struct e_list* list, int i);

// Adds an item to the end of the list.
// Retruns true on success, false on fail.
bool e_list_add(struct e_list* list, void* item);

// Inserts an item at index i into the list.
// Returns true on success, false on fail.
bool e_list_insert(struct e_list* list, void* item, int i);

// Returns the index of the first occurence of item in the list.
// Returns -1 if not found.
int e_list_find_index(struct e_list* list, void* item);

// Removes first occurence of item in the list.
// Returns true on success, false on fail.
bool e_list_remove(struct e_list* list, void* item);

// Removes item at index i in the list.
// Returns true on success, false on fail.
bool e_list_remove_index(struct e_list* list, int i);

// Frees the list, and memory for holding the item pointers, but not the items themselves.
void e_list_destroy(struct e_list* list);