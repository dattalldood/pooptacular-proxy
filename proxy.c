#include <stdio.h>
#include "csapp.h"
#include <assert.h>

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

static const char *user_agent = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *accept_line = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding = "Accept-Encoding: gzip, deflate\r\n";

void read_requesthdrs(rio_t *rp);
void parse_uri(char *uri, char *filename, char *host, int *port);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void clienterror(int fd, char *cause, char *errnum, 
	char *shortmsg, char *longmsg);
void get_header_info(int fd, char *method, char *version, char *host, 
    char *filename, int *port, char *request_buffer);
int is_valid_method(int fd, char *method);
void send_request_to_server(char *host, int port, char *request_buffer, int serverfd);

int main(int argc, char **argv){
    printf("%s%s%sport:%s \n", user_agent, accept_line, accept_encoding, argv[1]);
    int listenfd, port;
    socklen_t clientlen;
    struct sockaddr_in clientaddr;

    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(1);
    }
    port = atoi(argv[1]);
    
    listenfd = Open_listenfd(port);
    while (1) {
        int connfd, serverfd;

		clientlen = sizeof(clientaddr);
		connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
		char host[MAXLINE+1], method[MAXLINE];
        char version[MAXLINE], filename[MAXLINE];
        int port = 80;
        char request_buffer[MAXLINE*100];
		get_header_info(connfd, method, version, host, filename, &port, request_buffer);
        if (is_valid_method(connfd, method)){
            printf("connecting to: %s on port %d\n", host, port);
            serverfd = Open_clientfd(host, port);

            printf("request buffer\n%s\n", request_buffer);
            send_request_to_server(host, port, request_buffer, serverfd);
            char responsebuf[MAXLINE*100];
            printf("--------------------\n");
            rio_readn(serverfd, responsebuf, MAXLINE*100);
            printf("%s\n", responsebuf);
            Rio_writen(connfd, responsebuf, strlen(responsebuf));
            printf("==========================================================\n");
            Close(serverfd);
        }

        Close(connfd);
    }
    return 0;
}

int is_valid_method(int fd, char *method){
    if (strcasecmp(method, "GET")) {
       clienterror(fd, method, "501", "Not Implemented",
                "Tiny does not implement this method");
        return 0;
    }
    return 1;
}

void get_header_info(int fd, char *method, char *version, char *host, char *filename, int *port, char *request_buffer){
    char buf[MAXLINE], uri[MAXLINE];
    rio_t rio;
	Rio_readinitb(&rio, fd);
	Rio_readlineb(&rio, buf, MAXLINE);
	//reads the first line to get request line
	sscanf(buf, "%s %s %s", method, uri, version);
    parse_uri(uri, filename, host, port);
    //add default HTTP request info
    sprintf(request_buffer, "%s %s HTTP/1.0\r\n", method, filename);
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
    	char key[MAXLINE], value[MAXLINE];
    	sscanf(buf, "%s %s", key, value);
    	// extract the host 
    	if (!strcmp(key, "Host:")){
    		strncpy(host, value, MAXLINE);
    		printf("  host: %s\n\n", host);
            strcat(request_buffer, buf);
    	}
        else if (strcmp(key, "User-Agent:") && strcmp(key, "Accept:") && strcmp(key, "Accept-Encoding:")){
            //if key isnt user-agent, accept, or accept-encoding
            strcat(request_buffer, buf);
        }
    }
    strcat(request_buffer, "\r\n");
}

void send_request_to_server(char *host, int port, char *request_buffer, int serverfd){
    Rio_writen(serverfd, request_buffer, strlen(request_buffer));
}

/*
 * parse_uri - parse URI into filename and CGI args
 * return 0 if dynamic content, 1 if static
 */

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
	 
    printf("host: %s, filename: %s, port: %d\n",host, filename, *port);
}
/* $end parse_uri */

/*
 * serve_static - copy a file back to the client 
 */
/* $begin serve_static */
void serve_static(int fd, char *filename, int filesize) 
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];
 
    /* Send response headers to client */
    get_filetype(filename, filetype);       //line:netp:servestatic:getfiletype
    sprintf(buf, "HTTP/1.0 200 OK\r\n");    //line:netp:servestatic:beginserve
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    Rio_writen(fd, buf, strlen(buf));       //line:netp:servestatic:endserve

    /* Send response body to client */
    srcfd = Open(filename, O_RDONLY, 0);    
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    Close(srcfd);       
    Rio_writen(fd, srcp, filesize);  
    Munmap(srcp, filesize);          
}

/*
 * get_filetype - derive file type from file name
 */
void get_filetype(char *filename, char *filetype) 
{
    if (strstr(filename, ".html"))
	strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
	strcpy(filetype, "image/gif");
    else if (strstr(filename, ".jpg"))
	strcpy(filetype, "image/jpeg");
    else
	strcpy(filetype, "text/plain");
}  

/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}
/* $end clienterror */
