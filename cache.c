#include <stdio.h>
#include "csapp.h"
#include <assert.h>

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

typedef struct dll {
    struct dll *next;
    struct dll *prev;
    char *data;
} dll;


