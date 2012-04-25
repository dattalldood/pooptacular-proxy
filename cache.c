#include "cache.h"
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
    /* middle element; set pointers accordingly to remove elem from the list */
    else if (elem->next && elem->prev) {
        elem->prev->next = elem->next;
        elem->next->prev = elem->prev;
    }
    /* elem is the front element; set the next element to be front */
    else if (elem->next) {
        front = elem->next;
        elem->next->prev = NULL;
    }
    /* elem is the back element; set the previous element to be back */
    else if (elem->prev) {
        back = elem->prev;
        elem->prev->next = NULL;
    }
    /* if we store data, deleting the element will decrease the cache size */
    if (elem->resp)
        cachesize -= strlen(elem->resp);
    /* free elem */
    elem_free(elem);
}

void copybytes(char *dest, char *src, int datasize){
    int i;
    for (i=0; i<datasize; i++){
        dest[i] = src[i];
    }
}

/* insert a response into the cache. Returns 0 on success or -1 on failure */
int insert (char *request, char *response, int datasize) {

    int size = strlen(response);

    /* if there is not enough room in the cache, delete the least recently used
     * list elements until space is avalable */
    while (cachesize + size > MAX_CACHE_SIZE)
        delete(back);

    /* we have room in the cache for the object */
    char *creq = malloc(sizeof(char) * (strlen(request) + 1));
    char *cresp = malloc(datasize);
    dll *new = malloc(sizeof(dll));
    /* mallloc failure */
    if (!new || !creq || !cresp)
        return -1;
    strcpy(creq, request);
    copybytes(cresp, response, datasize);
    new->req = creq;
    new->resp = cresp;
    new->prev = NULL;
    new->datasize = datasize;
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
    /* elem is NULL; do nothing */
    if (!elem)
        return;
    /* elem is already the front element; no action is required */
    else if (!elem->prev)
        return;
    /* elem is the back list element; make the previous element the back */
    else if (!elem->next) {
        elem->prev->next = NULL;
        back = elem->prev;
    }
    /* elem is a middle element; remove and set pointers accordingly */
    else {
        elem->prev->next = elem->next;
        elem->next->prev = elem->prev;
    }
    /* make elem the front element */
    elem->prev = NULL;
    elem->next = front;
    front->prev = elem;
    front = elem;
}

/* looks for the given HTTP request string in the cache. if found, returns the
 * corresponding response and moves the containing list element to the front of
 * the list; otherwise returns NULL */
dll *lookup (char *request) {

    dll *current = front;

    /* search until we find the key or we reach the end of the list */
    while (current) {
        /* success! */
        if (current->req && !strcmp(request, current->req)) {
            update(current);
            return current;
        }
        /* these are not the droids we're looking for */
        else
            current = current->next;
    }

    /* search failed; return NULL */
    return NULL;
}
