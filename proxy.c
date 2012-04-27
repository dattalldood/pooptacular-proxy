#include <stdio.h>
#include "csapp.h"
#include <assert.h>
#include "cache.h"
#include <signal.h>

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

static const char *user_agent = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *accept_line = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding = "Accept-Encoding: gzip, deflate\r\n";

void parse_uri(char *uri, char *filename, char *host, int *port);
void get_header_info(int fd, char *method, char *version, char *host, 
    char *filename, int *port, char *request_buffer);
int is_valid_method(int fd, char *method);
void send_request_to_server(char *host, int port, char *request_buffer, int serverfd);
void *client_thread(void *arg);
void thread_safe_init();
void pipe_handler(int sig);

int main(int argc, char **argv){
    thread_safe_init();
    if (signal(SIGPIPE, pipe_handler) == SIG_ERR)
        unix_error("signal handler error\n");

    int listenfd, *connfd, port;
    pthread_t tid;
    struct sockaddr_in clientaddr;
	socklen_t clientlen = sizeof(clientaddr);

    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(1);
    }
    port = atoi(argv[1]);
    
    /* Accepts new connections and spawns new threads to deal with
     each connection */
    listenfd = Open_listenfd(port);
    while (1) {
        connfd = Malloc(sizeof(int));
		*connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Pthread_create(&tid, NULL, client_thread, connfd);
    }
    return 0;
}

/* Handler for SIGPIPE exceptions */
void pipe_handler(int sig) {
    return;
}

/* Determines if the requested method is supported */
int is_valid_method(int fd, char *method){
    if (strcasecmp(method, "GET")) {
        return 0;
    }
    return 1;
}

/* Initializes the semophores for use in protecting the cache */
void thread_safe_init(){
    Sem_init(&mutex, 0, 1); //mutex = 1
    Sem_init(&w, 0, 1); //mutex = 1
}

/* Handles a connection delegated to it by main. */
void *client_thread(void *arg){
    /* detach thread so we no longer need to manage it */
    if (pthread_detach(pthread_self()))
        unix_error("pthread detach error\n");

    int connfd = *((int *)arg);
    int serverfd;
    char host[MAXLINE+1], method[MAXLINE];
    char version[MAXLINE], filename[MAXLINE];
    int port = 80;
    char request_buffer[MAXLINE*100];
    int gtg = 1;
    Free(arg);

    get_header_info(connfd, method, version, host, filename, &port, request_buffer);

    /* make sure we have a GET request */
    if (is_valid_method(connfd, method)){
        sigset_t mask;
        /* if open server file descripter */
        if ((serverfd = open_clientfd(host, port)) < 0) {
            Close(connfd);
            return NULL;
        }
        /* lolcode :D
        if (strlen(filename) >= 4) {
            char *ext = index(filename, '?');
            if (ext && filename[0] != '?' && filename[1] != '?'
                && filename[2] != '?' && filename[3] != '?')
                ext = &ext[-4];
            else
                ext = &filename[strlen(filename) - 4];
            if (!strncmp(".jpg", ext, 4) || !strncmp(".png", ext, 4) || !strncmp(".gif", ext, 4)) {
                int fd = open("seal.png", O_RDONLY, 0);
                char imgbuf[64];
                int csize;
                while ((csize = rio_readn(fd, imgbuf, 64)) > 0)
                    if (rio_writen(connfd, imgbuf, csize) < 0)
                        return NULL;
                Close(connfd);
                return NULL;
            }
        }*/

        dll *cacheBlock;
        if ((cacheBlock = lookup(request_buffer)) != NULL){
            // if its in the cache
            Rio_writen(connfd, cacheBlock->resp, cacheBlock->datasize);
            Free(cacheBlock);
        }
        else {
            // if its not in the cache
            char cachebuf[MAX_OBJECT_SIZE];
            int datasize = 0;
            int responsebuf[64];
            int chunksize;
            send_request_to_server(host, port, request_buffer, serverfd);
            /* block SIGPIPE signals */
            Sigemptyset(&mask);
            Sigaddset(&mask, SIGPIPE);
            Sigprocmask(SIG_BLOCK, &mask, NULL);
            /* initiate read-write sequence */
            while ((chunksize = rio_readn(serverfd, responsebuf, 64)) > 0) {
                if (datasize+chunksize <= MAX_OBJECT_SIZE){
                    memcpy(cachebuf+datasize, responsebuf, chunksize);    
                }
                datasize += chunksize;
                if (rio_writen(connfd, responsebuf, chunksize) < 0) {
                    gtg = 0;
                    break;
                }
            }
            //add the data to the cache, only if it fits
            if (gtg && (datasize <= MAX_OBJECT_SIZE)){
                if (insert(request_buffer, cachebuf, datasize) < 0){
                    printf("cache insertion error\n");
                }   
            }
        }
        Sigprocmask(SIG_UNBLOCK, &mask, NULL);
        Close(serverfd);
    }
    Close(connfd);
    return NULL;
}

/* Processes the request sent from the client. Takes the opened file descriptior
 * and returns all the necessary information from it. 
 */
void get_header_info(int fd, char *method, char *version, char *host, 
    char *filename, int *port, char *request_buffer){
    char buf[MAXLINE], uri[MAXLINE];
    rio_t rio;
	
    Rio_readinitb(&rio, fd);
    //reads the first line to get the request line
	Rio_readlineb(&rio, buf, MAXLINE);
	sscanf(buf, "%s %s %s", method, uri, version);
    parse_uri(uri, filename, host, port);
    //add default HTTP request info
    sprintf(request_buffer, "%s http://%s%s HTTP/1.0\r\n", method, host, filename);
    strcat(request_buffer, "Host: ");
    strcat(request_buffer, host);
    strcat(request_buffer, "\r\n");
    strcat(request_buffer, user_agent);
    strcat(request_buffer, accept_line);
    strcat(request_buffer, accept_encoding);
    /* Read headers */
    while (1){
	    Rio_readlineb(&rio, buf, MAXLINE);
	    if (!strcmp(buf,"\r\n")) break;
        if (strstr(buf, "User-Agent:")==NULL && strstr(buf, "Accept:")==NULL && 
            strstr(buf, "Accept-Encoding:")==NULL && strstr(buf, "Host:") == NULL){
            //if key isnt head, user-agent, accept, or accept-encoding
            strcat(request_buffer, buf);
        }
    }

    strcat(request_buffer, "\r\n");
}

void send_request_to_server(char *host, int port, char *request_buffer, int serverfd){
    Rio_writen(serverfd, request_buffer, strlen(request_buffer));
}

/* Takes the uri, and returns the filename, host, and port. */

void parse_uri(char *uri, char *filename, char *host, int *port) 
{
    char *ptr;
    int httplength = 7;

    if (uri == strstr(uri, "http://")){
        //if http (ie. http://google.com:80/index)
        char *pathStart = index(&uri[httplength], '/');
        if (pathStart == NULL){
            strncpy(filename, "/", 2);
            strncpy(host, &uri[httplength], MAXLINE);
        }
        else {
            strncpy(filename, pathStart, MAXLINE);    
            strncpy(host, &uri[httplength], pathStart-&uri[httplength]);
            host[(pathStart-&uri[httplength])] = '\0';
        }
        
        strcat(host, ""); //to put null character at end
    }
    else {
        // if no http (ie. google.com:80/index)
        char *pathStart = index(uri, '/');
        if (pathStart == NULL){
            strncpy(filename, "/", 2);
            strncpy(host, uri, MAXLINE);
        }
        else {
            strncpy(filename, pathStart, MAXLINE);
            strncpy(host, uri, pathStart-uri);
        }
    }

    if ((ptr = index(host, ':')) != NULL){
        //if there is a port specified
        char portString[100];
        strncpy(portString, ptr+1, 100);
        *port = atoi(portString);
        ptr[0] = '\0';
    }
}
