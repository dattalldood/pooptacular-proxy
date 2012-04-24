CC = gcc
CFLAGS = -g -Wall -Werror
LDFLAGS = -lpthread

all: proxy

csapp.o: csapp.c csapp.h
	$(CC) $(CFLAGS) -c csapp.c

cache.o: cache.c csapp.h
	$(CC) $(CFLAGS) -c cache.c

proxy.o: proxy.c csapp.h
	$(CC) $(CFLAGS) -c proxy.c

proxy: proxy.o cache.o csapp.o

submit:
	(make clean; cd ..; tar czvf proxylab.tar.gz proxylab-handout)

clean:
	rm -f *~ *.o proxy core

