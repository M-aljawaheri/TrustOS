#include<stdint.h>
#include<stdbool.h>

#ifndef LISTS
#define LISTS


struct list {
    void *data;
    struct list *next;
};

typedef struct list list_node;
typedef list_node* list_t;

/*
 * all linear lists have a dummy node with null data and null next at the tail
 */
list_t create_list();

/*
 * all circular lists are initialized with the data
 * NOTE: single node circular lists point to themselves as next
 */
list_t create_circular_list(void *data);

/*
 * inserts a new node as the next of current node
 * returns a pointer to the latest added node
 * 
 * Note: be careful when adding as next to an empty list, 
 * you need to update any pointers to the empty (dummy) node to 
 * become the node you just added. In general, I recommend using this 
 * function only when your list is non-empty.
 * 
 */
list_t add_as_next(list_t current_node, void *data);

/*
 * adds to the front of list / current node of circular list
 */
list_t add_to_front(list_t lst, void *data);

/*
 *  REQUIRES: list is not a circular list (has a tail)
 */
list_t add_to_back(list_t lst, void *data);

/*
 * REQUIRES: list is in the node
 * Returns new list with node removed
 * TODO: add prev pointers to make this efficient
 */
list_t delete_node(list_t lst, list_t node);

#endif