#include <stdio.h>
#include "csapp.h"
#include <assert.h>

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

typedef struct dll {
  int datasize;
  struct dll *next;
  struct dll *prev;
  char *req;
  char *resp;
} dll;

void copybytes(char *dest, char *src, int datasize);
dll *lookup (char *request);
int insert (char *request, char *response, int datasize);

sem_t mutex, w;