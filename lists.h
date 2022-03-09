#include<stdint.h>
#include<stdbool.h>

struct list {
    char *data;
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
list_t create_circular_list(char *data);

/*
 * inserts a new node as the next of current node
 */
list_t add_as_next(list_t current_node, char *data);

/*
 * adds to the front of list / current node of circular list
 */
list_t add_to_front(list_t lst, char *data);

/*
 *  REQUIRES: list is not a circular list (has a tail)
 */
list_t add_to_back(list_t lst, char *data);