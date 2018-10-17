/*
 * proxy.c - ICS Web proxy
 * 
 * Chen Zhiyang - 516030910347
 */

#include "csapp.h"
#include <stdarg.h>
#include <sys/select.h>

/* 
 * A structure to store client address and connected descriptor,
 * as a packed argument to pass to threads.
 */
typedef struct client_item_t
{
    struct sockaddr_in clientaddr;
    int connfd;
} client_item;

/*
 * Mutex semaphores for thread synchronization.
 */
sem_t mutex_host, mutex_file;

/*
 * Function prototypes
 */
int parse_uri(char *uri, char *target_addr, char *path, char *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size);
ssize_t Rio_readn_w(int fd, void *ptr, size_t nbytes);
void Rio_writen_w(int fd, void *usrbuf, size_t n);
ssize_t Rio_readnb_w(rio_t *rp, void *usrbuf, size_t n);
ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen);
void *doit(void *vargp);
/*
 * main - Main routine for the proxy program
 */
int main(int argc, char **argv)
{
    int listenfd;
    client_item *cip;
    socklen_t clientlen = sizeof(struct sockaddr_in);
    pthread_t tid;

    /* Check arguments */
    if (argc != 2) {
       fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
       exit(0);
    }

    /* Ignore SIGPIPE */
    Signal(SIGPIPE, SIG_IGN);

    /* Initialize mutex semaphores */
    Sem_init(&mutex_host, 0, 1);
    Sem_init(&mutex_file, 0, 1);

    /* Create a listen descriptor */
    listenfd = Open_listenfd(argv[1]);
    while (1) {
        /* Allocate new space to store client_item objects (guarantee thread safety) */
        cip = Malloc(sizeof(client_item));

        /* Wait for a connection request */
        cip->connfd = Accept(listenfd, (SA *)&cip->clientaddr, &clientlen);

        /* Handle the request in a new thread */
        Pthread_create(&tid, NULL, doit, cip);
    }

    exit(0);
}


/*
 * parse_uri - URI parser
 * 
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int parse_uri(char *uri, char *hostname, char *pathname, char *port)
{
    char *hostbegin;
    char *hostend;
    char *pathbegin;
    int len;

    if (strncasecmp(uri, "http://", 7) != 0) {
        hostname[0] = '\0';
        return -1;
    }

    /* Extract the host name */
    hostbegin = uri + 7;
    hostend = strpbrk(hostbegin, " :/\r\n\0");
    if (hostend == NULL)
        return -1;
    len = hostend - hostbegin;
    strncpy(hostname, hostbegin, len);
    hostname[len] = '\0';

    /* Extract the port number */
    if (*hostend == ':') {
        char *p = hostend + 1;
        while (isdigit(*p))
            *port++ = *p++;
        *port = '\0';
    } else {
        strcpy(port, "80");
    }

    /* Extract the path */
    pathbegin = strchr(hostbegin, '/');
    if (pathbegin == NULL) {
        pathname[0] = '\0';
    }
    else {
        pathbegin++;
        strcpy(pathname, pathbegin);
    }

    return 0;
}
/*
 * format_log_entry - Create a formatted log entry in logstring.
 * 
 * The inputs are the socket address of the requesting client
 * (sockaddr), the URI from the request (uri), and the size in bytes
 * of the response from the server (size).
 */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size)
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
    sprintf(logstring, "%s: %d.%d.%d.%d %s %d", time_str, a, b, c, d, uri, size);
}

/*
 * A new wrapper function for rio_readn().
 * Instead of terminating the program when operation fails, it only prints an error message.
 */
ssize_t Rio_readn_w(int fd, void *ptr, size_t nbytes)
{
    ssize_t n;

    if ((n = rio_readn(fd, ptr, nbytes)) < 0) {
        printf("Rio_readn_w failed\n");
        return 0;
    }
    return n;
}

/*
 * A new wrapper function for rio_writen().
 * Instead of terminating the program when operation fails, it only prints an error message.
 */
void Rio_writen_w(int fd, void *usrbuf, size_t n)
{
    if (rio_writen(fd, usrbuf, n) != n)
        printf("Rio_writen_w failed\n");
}

/*
 * A new wrapper function for rio_readnb().
 * Instead of terminating the program when operation fails, it only prints an error message.
 */
ssize_t Rio_readnb_w(rio_t *rp, void *usrbuf, size_t n)
{
    ssize_t rc;

    if ((rc = rio_readnb(rp, usrbuf, n)) < 0) {
        printf("Rio_readnb_w failed\n");
        return 0;
    }
    return rc;
}

/*
 * A new wrapper function for rio_readlineb().
 * Instead of terminating the program when operation fails, it only prints an error message.
 */
ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen)
{
    ssize_t rc;

    if ((rc = rio_readlineb(rp, usrbuf, maxlen)) < 0) {
        printf("Rio_readlineb_w failed\n");
        return 0;
    }
    return rc;
}
     
/*
 * Thread function.
 * Forward request to target server, and forward response back to client.
 * If a response is received, a log line will be created.
 */
void *doit(void *vargp)
{
    size_t n; /* Number of bytes transmitted at one time */
    char buf[MAXLINE]; /* Line buffer */
    char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char hostname[MAXLINE], pathname[MAXLINE],port[MAXBUF];
    char logstring[MAXLINE];
    int connfd, clientfd;
    int body_len = 0x7fffffff; /* Length of body detected by "Content-length" header */
    int recv_body_len = 0,req_body_len=0; /* Received body length */
    int recv_len = 0; /* Received total length */
    int body_len_set = 0; /* Whether body length has been detected */
    rio_t rio_conn, rio_client;
    client_item ci;
    FILE *logfile;

    /* Detach this thread */
    Pthread_detach(pthread_self());

    /* Get argument and free the allocated space */
    ci = *((client_item *)vargp);
    connfd = ci.connfd;
    Free(vargp);

    /* Get the first request line and parse some data */
    Rio_readinitb(&rio_conn, connfd);
    if (Rio_readlineb_w(&rio_conn, buf, MAXLINE) == 0) {
        Close(connfd);
        return NULL;
    }
    if (sscanf(buf, "%s %s %s", method, uri, version) != 3) {
        printf("request line format error\n");
        Close(connfd);
        return NULL;
    }

    if (parse_uri(uri, hostname, pathname, port) < 0) {
        printf("parse_uri error\n");
        Close(connfd);
        return NULL;
    }


//P(&mutex_host);
    clientfd=open_clientfd(hostname,port);
//V(&mutex_host);
if (clientfd==-1){
	Close(connfd);
        return NULL;
}
    Rio_readinitb(&rio_client, clientfd);
    sprintf(buf, "%s /%s HTTP/1.1\r\n", method, pathname);
    Rio_writen_w(clientfd, buf, strlen(buf));

    /* Forward remaining request lines to target server */
    while ((n = Rio_readlineb_w(&rio_conn, buf, MAXLINE)) != 0) {
        Rio_writen_w(clientfd, buf, n);
        if (!body_len_set&&!strncasecmp(buf, "Content-length:", 15)) {
            sscanf(buf + 15, "%d", &body_len);
            body_len_set=1;
        }
        if (!strcmp(buf, "\r\n")) 
            break;
    }
   if (n==0){
      Close(clientfd);
      Close(connfd);
      return NULL;
   }

    if(body_len_set){
        while (n=Rio_readnb_w(&rio_conn, buf, 1)!=0){
             Rio_writen_w(clientfd, buf, n);
             req_body_len += n;

        /* Detect end of response */
        if (req_body_len >= body_len) /* End of response (given "Content-length") */
            break;
   }
}
   if (n==0){
      Close(clientfd);
      Close(connfd);
      return NULL;

   }
    body_len_set=0;
    /* Receive response from target server and forward to client */
    n=Rio_readlineb_w(&rio_client, buf, MAXLINE);
    if (n==0){
      Close(clientfd);
      Close(connfd);
      return NULL;
   }

    while (strcmp(buf,"\r\n")) {
        /* Fix bug of return value when response line exceeds MAXLINE */
        if (n == MAXLINE && buf[MAXLINE - 2] != '\n')
            --n;
        /* Send the response line to client */
        Rio_writen_w(connfd, buf, n);
        recv_len += n;
       if (!body_len_set && !strncasecmp(buf, "Content-length:", 15)) {
            sscanf(buf + 15, "%d", &body_len);
            body_len_set = 1;
        }
      n=Rio_readlineb_w(&rio_client, buf, MAXLINE);
      if (n==0){
      Close(clientfd);
      Close(connfd);
      return NULL;

   }

    }
     Rio_writen_w(connfd, "\r\n", n);
    recv_len+=n;

/*body*/
     while (n=Rio_readnb_w(&rio_client, buf, 1)!=0){
    
        Rio_writen_w(connfd, buf, 1);

        /* Calculate length and detect body */
        recv_len += n;
        recv_body_len += n;
       

        /* Detect end of response */
        if (recv_body_len >= body_len) /* End of response (given "Content-length") */
            break;
        if (!strcmp(buf, "0\r\n")) { /* End of response (chunked) */
            Rio_writen_w(connfd, "\r\n", 2);
            recv_len += 2;
            break;
        }
   }
   if (n==0){
      Close(clientfd);
      Close(connfd);
      return NULL;

   }
    

    /* Close connection with target server */
    Close(clientfd);

    /* Generate log line */
    format_log_entry(logstring, &ci.clientaddr, uri, recv_len);
   
    /* Write to log file (protected by a mutex semaphore) */
    P(&mutex_file);
    logfile = fopen("proxy.log", "a");
 printf("%s\n",logstring);
    fprintf(logfile, "%s\n",logstring);
    fclose(logfile);
    V(&mutex_file);

    /* Close connection with client */
    Close(connfd);

    return NULL;
}
