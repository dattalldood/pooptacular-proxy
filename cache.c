#include <stdio.h>
#include "csapp.h"
#include <assert.h>

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

typedef struct dll {
    struct dll *next;
    struct dll *prev;
    char *req;
    char *resp;
} dll;

static dll *front = NULL;
static dll *back = NULL;

static int cachesize = 0;

/* frees the given list element */
static void elem_free (dll *elem) {
    /* elem is NULL; do nothing */
    if (!elem)
        return;
    /* free all allocated memory */
    free(elem->req);
    free(elem->resp);
    free(elem);
}

/* removes the given element from the list */
static void delete (dll *elem) {
    /* elem is NULL; do nothing */
    if (!elem)
        return;
    /* previous and next element; set pointers accordingly to remove elem from
     * the list */
    else if (elem->next && elem->prev) {
        elem->prev->next = elem->next;
        elem->next->prev = elem->prev;
    }
    /* elem is the first element; set the next element to be first */
    else if (elem->next)
        elem->next->prev = NULL;
    /* elem is the last element; set the previous element to be last */
    else if (elem->prev)
        elem->prev->next = NULL;
    /* if we store data, deleting the element will decrease the cache size */
    if (elem->resp)
        cachesize -= strlen(elem->resp);
    /* free elem */
    elem_free(elem);
}

/* insert a response into the cache. Returns 0 on success or -1 on failure */
int insert (char *request, char *response) {

    int size = strlen(response);

    /* if there is not enough room in the cache, delete the least recently used
     * list elements until space is avalable */
    while (cachesize + size > MAX_CACHE_SIZE)
        delete(back);

    /* we have room in the cache for the object */
    char *creq = malloc(sizeof(char) * (strlen(request) + 1));
    char *cresp = malloc(sizeof(char) * (strlen(response) + 1));
    dll *new = malloc(sizeof(dll));
    /* mallloc failure */
    if (!new || !creq || !cresp)
        return -1;
    strcpy(creq, request);
    strcpy(cresp, response);
    new->req = creq;
    new->resp = cresp;
    new->prev = NULL;
    /* empty cache. start from scratch */
    if (!front) {
        new->next = NULL;
        front = back = new;
    }
    else {
        new->next = front;
        front->prev = new;
        front = new;
    }
    cachesize += size;

    /* success */
    return 0;
}

/* moves the given element to the front of the list */
static void update (dll *elem) {
    //TODO
    return;
}

/* looks for the given HTTP request string in the cache. if found, returns the
 * corresponding response and moves the containing list element to the front of
 * the list; otherwise returns NULL */
char *lookup (char *request) {
    //TODO
    update(NULL);
    return NULL;
}
