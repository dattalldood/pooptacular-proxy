#include "cache.h"
static dll *front = NULL;
static dll *back = NULL;

typedef struct ins_args {
    char *req;
    char *resp;
    int size;
} ins_args;

/* Concurrency setup */
volatile int readcnt = 0;


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
        cachesize -= elem->datasize;
    /* free elem */
    elem_free(elem);
}

/* checks to see if the given data is present in the cache. does not update */
static dll *search (char *request) {
    dll *current = front;
    while (current)
        if (current->req && !strcmp(request, current->req))
            return current;
        else
            current = current->next;
    return NULL;
}

/* separate runtime to allow a client to continue receiving requests without
 * having to wait for the write to go through */
static void *thread_insert (void *args) {

    /* detach thread so we no longer have to manage it */
    Pthread_detach(Pthread_self());

    ins_args *info = (ins_args *)args;
    char *request = info->req;
    char *response = info->resp;
    int datasize = info->size;

    //Concurrency: insert is a writer
    P(&w);

    /* if data was written while we waited, do nothing */
    if (search(request)) {
        V(&w);
        return NULL;
    }

    /* if there is not enough room in the cache, delete the least recently used
     * list elements until space is avalable */
    while (cachesize + datasize > MAX_CACHE_SIZE){
        delete(back);
    }

    /* we have room in the cache for the object */
    char *creq = Malloc(sizeof(char) * (strlen(request) + 1));
    char *cresp = Malloc(datasize);
    dll *new = Malloc(sizeof(dll));
    strcpy(creq, request);
    memcpy(cresp, response, datasize);
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
    cachesize += datasize;

    V(&w);
    return NULL;
}

/* packs the arguments for insert into one structure for passage to 
 * thread_insert */
static ins_args *pack (char *request, char *response, int datasize) {
    ins_args *args = Malloc(sizeof(ins_args));
    args->req = request;
    args->resp = response;
    args->size = datasize;
    return args;
}

/* insert a response into the cache. Returns 0 on success or -1 on failure */
int insert (char *request, char *response, int datasize) {

    pthread_t tid;

    /* if our data is already in the cache do not write */
    if (lookup(request))
        return 0;
    
    Pthread_create(&tid, NULL, thread_insert,
                   pack(request, response, datasize));

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
    // Concurrency: lookup is a reader
    P(&mutex);
    readcnt++;
    if (readcnt == 1)
        P(&w);
    V(&mutex);
    dll *current = front;

    /* search until we find the key or we reach the end of the list */
    while (current) {
        /* success! */
        if (current->req && !strcmp(request, current->req)) {
            update(current);

            P(&mutex);
            readcnt--;
            if (readcnt == 0)
                V(&w);
            V(&mutex);
            return current;
        }
        /* these are not the droids we're looking for */
        else
            current = current->next;
    }

    P(&mutex);
    readcnt--;
    if (readcnt == 0)
        V(&w);
    V(&mutex);

    /* search failed; return NULL */
    return NULL;
}
