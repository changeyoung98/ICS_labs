/*
 * proxy.c - ICS Web proxy
 *
 *
 */

#include "csapp.h"
#include <stdarg.h>
#include <sys/select.h>



/*
 * Function prototypes
 */
int parse_uri(char *uri, char *target_addr, char *path, char *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, size_t size);

ssize_t Rio_readn_w(int fd, void *ptr, size_t nbytes);
void Rio_writen_w(int fd, void *usrbuf, size_t n);
ssize_t Rio_readnb_w(rio_t *rp, void *usrbuf, size_t n);
ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen);
void echo(int connfd);

   int main(int argc, char **argv)
  {
 	int listenfd, connfd;
 	socklen_t clientlen;
 	struct sockaddr_storage clientaddr; /* Enough space for any address */
 	char client_hostname[MAXLINE], client_port[MAXLINE];
 	if (argc != 2) {
 	      fprintf(stderr, "usage: %s <port>\n", argv[0]);
 	      exit(0);
 	}
        
        listenfd = Open_listenfd(argv[1]);
 	while (1) {
 	      clientlen = sizeof(struct sockaddr_storage);
 	      connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
 	      Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, 
 			 MAXLINE, client_port, MAXLINE, 0);
 	      printf("Connected to (%s, %s)\n", client_hostname, client_port);
 	      echo(connfd);
 	      Close(connfd);
 	}
 	exit(0);
  }
void echo(int connfd)
  {
    	size_t n;
    	char buf[MAXLINE];
    	rio_t rio;

    	Rio_readinitb(&rio, connfd);
    	while((n = Rio_readlineb(&rio, buf, MAXLINE))!=0){
        	      printf("server received %d bytes\n", n);
        	      Rio_writen(connfd, buf, n);
    	}
}








/*
 * Rio - new Rio functions
 */
ssize_t Rio_readn_w(int fd, void *ptr, size_t nbytes) {
    ssize_t n;

    if ((n = rio_readn(fd, ptr, nbytes)) < 0) {
        n = 0;
        printf("Rio_readn error");
    }
    return n;
}

void Rio_writen_w(int fd, void *usrbuf, size_t n)
{
    if (rio_writen(fd, usrbuf, n) != n) {
        printf("Rio_writen error");
    }
}
ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen)
{
    ssize_t rc;

    if ((rc = rio_readlineb(rp, usrbuf, maxlen)) < 0) {
        rc = 0;
        printf("Rio_readlineb error");
    }
    return rc;
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


void format_log_entry(char *logstring, struct sockaddr_in *sockaddr,
                      char *uri, size_t size)
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
     * returns a pointer to a static variable (Ch 12, CS:APP).
     */
    host = ntohl(sockaddr->sin_addr.s_addr);
    a = host >> 24;
    b = (host >> 16) & 0xff;
    c = (host >> 8) & 0xff;
    d = host & 0xff;

    /* Return the formatted log entry string */
    sprintf(logstring, "%s: %d.%d.%d.%d %s %zu", time_str, a, b, c, d, uri, size);
}

