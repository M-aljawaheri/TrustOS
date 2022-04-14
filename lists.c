#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include "staticMalloc.h"

struct list {
    void *data;
    struct list *next;
    struct list *prev;
};

typedef struct list list_node;
typedef list_node* list_t;

/*
 * all linear lists have a dummy node with NULL data and NULL next/prev at the tail
 */
list_t create_list() {
    list_t res = MALLOC(sizeof(list_node));
    if (!res) return NULL;
    res->data = NULL;
    res->next = NULL;
    res->prev = NULL;
    return res;
}

/*
 * all circular lists are initialized with the data
 * NOTE: single node circular lists point to themselves as next
 */
list_t create_circular_list(void *data) {
    list_t res = MALLOC(sizeof(list_node));
    if (!res) return NULL;
    res->data = data;
    res->next = res;
    res->prev = res;
    return res;
}

/*
 * inserts a new node as the next of current node
 * returns a pointer to the latest added node
 * Note: be careful when adding as next to an empty list
 */
list_t add_as_next(list_t lst, void *data) {
    list_t node =  MALLOC(sizeof(list_node));
    if (!node) return NULL;
    node->data = data;
    //if empty list, add in front of dummy node
    if (!lst->next) {
        node->next = lst;
        node->prev = NULL;
        lst->prev = node;
        return node;
    }
    //else, add after current lst node
    node->next = lst->next;
    node->prev = lst;
    node->next->prev = node;
    lst->next = node;
    return lst;
}

/*
 * adds to the front of list / current node of circular list
 */
list_t add_to_front(list_t lst, void *data) {
    list_t node =  MALLOC(sizeof(list_node));
    if (!node) return NULL;
    node->next = lst;
    node->prev = NULL;
    node->data = data;
    lst->prev = node;
    return node;
}

/*
 *  REQUIRES: list is not a circular list (has a tail)
 */
list_t add_to_back(list_t lst, void *data) {
    list_t node =  MALLOC(sizeof(list_node));
    if (!node) return NULL;
    node->data = data;
    list_t current_node = lst;
    //special case: adding to an empty list: add in front of dummy node
    if (!(current_node->next)) {
        node->next = lst;
        node->prev = NULL;
        current_node->prev = node;
        return node;
    }
    //loop invariant: current_node->next is never null
    while (current_node->next->next) {
        current_node = current_node->next;
    }
    //current node is the node before the dummy node
    node->next = current_node->next;
    node->prev = current_node;
    node->next->prev = node;
    current_node->next = node;
    return lst;
}

/*
 * Returns the next node of the deleted node
 * Returns NULL if your delete caused the entire list to be deleted 
 * (useful for final clean up of dummy node/singleton circular list)
 */
list_t delete_node(list_t node) {
    if (node == NULL) return NULL;
    //dummy node in an empty list OR singleton node in circular list
    if (node->next == NULL || node->next == node) {
        FREE(node);
        return NULL;
    }
    //default cases
    if (node->prev == NULL) {
        node->next->prev = NULL;
    }
    else {
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }
    list_t res = node->next;
    FREE(node);
    return res;
}