#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

struct list {
    char *data;
    struct list *next;
};

typedef struct list list_node;
typedef list_node* list_t;

/*
 * all linear lists have a dummy node with NULL data and NULL next at the tail
 */
list_t create_list() {
    list_t res = malloc(sizeof(list_node));
    if (!res) return NULL;
    res->data = NULL;
    res->next = NULL;
    return res;
}

/*
 * all circular lists are initialized with the data
 * NOTE: single node circular lists point to themselves as next
 */
list_t create_circular_list(char *data) {
    list_t res = malloc(sizeof(list_node));
    if (!res) return NULL;
    res->data = data;
    res->next = res;
    return res;
}

/*
 * inserts a new node as the next of current node
 * returns a pointer to the latest added node
 * Note: be careful when adding as next to an empty list
 */
list_t add_as_next(list_t lst, char *data) {
    list_t node =  malloc(sizeof(list_node));
    if (!node) return NULL;
    node->data = data;
    if (!lst->next) {
        node->next = lst;
        return node;
    }
    node->next = lst->next;
    lst->next = node;
    return node;
}

/*
 * adds to the front of list / current node of circular list
 */
list_t add_to_front(list_t lst, char *data) {
    list_t node =  malloc(sizeof(list_node));
    if (!node) return NULL;
    node->next = lst;
    node->data = data;
    return node;
}

/*
 *  REQUIRES: list is not a circular list (has a tail)
 */
list_t add_to_back(list_t lst, char *data) {
    list_t node =  malloc(sizeof(list_node));
    if (!node) return NULL;
    list_t current_node = lst;
    if (!current_node->next) {
        node->data = data;
        node->next = lst;
        return node;
    }
    while (current_node->next->next) {
        current_node = current_node->next;
    }
    node->next = current_node->next;
    current_node->next = node;
    node->data = data;
    return lst;
}

/*
 * REQUIRES: list is in the node
 * Returns new list with node removed
 * TODO: add prev pointers to make this efficient
 */
list_t delete_node(list_t lst, list_t node) {
    list_t current_node = lst;
    if (current_node->next->next == NULL) {
        return current_node->next;
    }
    while (current_node->next != node) {
        current_node = current_node->next;
    }
    current_node->next = node->next;
    //free next
    return lst;
}