#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
static const char *content_length_hdr_strt = "Content-length: ";
sem_t mutex_host, mutex_file;
void handle_request(int fd);
void* handle_request_threaded(void *vargp);
void process_hdrs(rio_t *rp, char *headers, char *hostname);
void process_answer(rio_t *rp, int fd);
void process_uri(char *uri, char *hostname, char *arguments, char *port);
void clienterror(int fd, char *cause, char *errnum, 
		 			char *shortmsg, char *longmsg);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size);
int main(int argc, char **argv) 
{
    int listenfd, *connfdp;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

   /* printf("%s\n", user_agent_hdr);*/

    /* Check command line args - RUDIMENTARY */
    if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(1);
    }

    listenfd = Open_listenfd(argv[1]);

    while (1) {
		clientlen = sizeof(clientaddr);
		connfdp = Malloc(sizeof(int));
		*connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
		Pthread_create(&tid, NULL, handle_request_threaded, connfdp);
    }
}

void *handle_request_threaded(void *vargp)
{
	int connfd = *((int *) vargp);
	Pthread_detach(pthread_self());
	Free(vargp);
	handle_request(connfd);
	Close(connfd);
	return NULL;
}

/*
 * handle_request - handle one HTTP request/response transaction
 */
void handle_request(int fd) 
{
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char headers[MAXBUF],type[MAXLINE],logstring[MAXLINE];
    char request[MAXLINE], hostname[MAXLINE], arguments[MAXLINE], port[MAXLINE];
    int serverfd;
    rio_t rio_client;
    rio_t rio_server;

    /* Read request line and headers */
    Rio_readinitb(&rio_client, fd);
    if (!Rio_readlineb(&rio_client, buf, MAXLINE))
        return;
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);
    if (strcasecmp(method, "GET")) {
        clienterror(fd, method, "501", "Not Implemented", "Proxy does not implement this method");
        return;
    }

    process_uri(uri, hostname, arguments, port);
    process_hdrs(&rio_client, headers, hostname);
    printf("%s\n", request);
    printf("%s\n", headers);

    sprintf(request, "GET %s HTTP/1.1\r\n", arguments);
   

    /* send request to server */
    serverfd = Open_clientfd(hostname, port);

    Rio_writen(serverfd, request, strlen(request));
    Rio_writen(serverfd, headers, strlen(headers));

    /* answer to client */
    Rio_readinitb(&rio_server, serverfd);
    process_answer(&rio_server, fd); 
/* Close connection with target server */
    Close(serverfd);

    /* Generate log line */
    format_log_entry(logstring, &serverfd, uri, MAXLINE);
FILE *logfile;
    /* Write to log file (protected by a mutex semaphore) */
    P(&mutex_file);
    logfile = fopen("proxy.log", "a");
    fprintf(logfile, "%s\n", logstring);
    fclose(logfile);
    V(&mutex_file);

    /* Close connection with client */
   

    return NULL; 
}

void process_uri(char *uri, char *hostname, char *arguments, char *port)
{
    strcpy(port, "80");

    /* ignore http:// */
    int offset = 0;
    if (strcasestr(uri, "http://")) {
        offset += 7;
    }

    /* no hostname in request */
    if (uri[offset] == '/') {
        hostname = NULL;
        strcpy(arguments, uri + offset);
    } else {
        /* there is a hostname */
        char *hostname_end = strstr(uri + offset, "/");
        if (hostname_end) {
            hostname_end[0] = '\0';
            strcpy(hostname, uri + offset);
            hostname_end[0] = '/';
            strcpy(arguments, hostname_end);
        }

        char *port_start = strstr(hostname, ":");
        if (port_start) {
        	/* we want the hostname without the port */
        	port_start[0] = '\0';
            strcpy(port, port_start + 1);
        }
    }
}

/*
 * process_hdrs - read the client HTTP request headers and add / replace with own headers
 */
void process_hdrs(rio_t *rp, char *hdrstr, char *hostname){
    char buf[MAXLINE];
    int provided_host = 0;
    
    Rio_readlineb(rp, buf, MAXLINE);
    while (strcmp(buf, "\r\n")) {      
            if (strstr(buf, "Accept-Charset")||strstr(buf, "Host")||strstr(buf, "Connection")) {
                provided_host = 1;
            }
            strcat(hdrstr, buf);

        Rio_readlineb(rp, buf, MAXLINE);
    if (!provided_host) {
        sprintf(hdrstr, "%s%s: %s\r\n", hdrstr,buf, hostname);
    }
}
    strcat(hdrstr, "\r\n");
    return;


}

void process_answer(rio_t *rio_server, int fd) {
	char buf[MAXLINE], content_buf[MAX_OBJECT_SIZE];
	int l = 0;

	/* Extract content-length and pass through headers */
	do {
		Rio_readlineb(rio_server, buf, MAXLINE);
        if (strcasestr(buf, content_length_hdr_strt))
            sscanf(buf + strlen(content_length_hdr_strt), "%d", &l);
        Rio_writen(fd, buf, strlen(buf));
    } while(strcmp(buf, "\r\n"));

    /* pass through content */
    Rio_readnb(rio_server, content_buf, l);
    Rio_writen(fd, content_buf, l);
}

/*
 * clienterror - returns an error message to the client
 */
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Proxy Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Proxy</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, 
		      char *uri, int size)
{
    time_t now;
    char time_str[MAXLINE];
    unsigned long host;
    unsigned char a, b, c, d;

    /* Get a formatted time string */
    now = time(NULL);
    strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

    /* 
     * Convert the IP address in network byte order to dotted decimal
     * form. Note that we could have used inet_ntoa, but chose not to
     * because inet_ntoa is a Class 3 thread unsafe function that
     * returns a pointer to a static variable (Ch 13, CS:APP).
     */
    host = ntohl(sockaddr->sin_addr.s_addr);
    a = host >> 24;
    b = (host >> 16) & 0xff;
    c = (host >> 8) & 0xff;
    d = host & 0xff;


    /* Return the formatted log entry string */
    sprintf(logstring, "%s: %d.%d.%d.%d %s", time_str, a, b, c, d, uri);
}
