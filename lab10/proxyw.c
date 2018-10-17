/*
 * proxy.c - ICS Web proxy
 * 
 * Wang Mengyao - 516030910177
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
ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen);
ssize_t Rio_readnb_w(rio_t *rp, void *usrbuf, size_t n);
void Rio_writen_w(int fd, void *usrbuf, size_t n);
void* thread(void *vargp);
void* echo(void *vargp);
typedef struct client_argv_t{
    struct sockaddr_in clientaddr;
    int connfd;
} client_argv;

sem_t f;
sem_t c;
//sem_t debug;
/*
 * main - Main routine for the proxy program
 */
int main(int argc, char **argv)
{
    /* Check arguments */
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
        exit(0);
    }

    Signal(SIGPIPE, SIG_IGN);
    Sem_init(&f,0,1);
	Sem_init(&c,0,1);
	//Sem_init(&debug,0,1);

    int listenfd, connfd;
    socklen_t clientlen;
    //struct sockaddr_storage clientaddr;
    struct sockaddr_in clientaddr;
    //char *host, *port;
    pthread_t tid;
    client_argv * ca;

    listenfd=Open_listenfd(argv[1]);

    while (1){
        clientlen=sizeof(struct sockaddr_in);
        connfd=Accept(listenfd, (SA *) & clientaddr, &clientlen);
        ca=Malloc(sizeof(client_argv));
        ca->connfd=connfd;
        ca->clientaddr=clientaddr;
        Pthread_create(&tid,NULL,thread,ca);
    }
    exit(0);
}


void *thread(void *vargp){

    Pthread_detach(pthread_self());

    //FILE *debugfile=Fopen("debug.txt","a");
    //char de[MAXLINE];

//get the vargp
    client_argv ca=*((client_argv *)vargp);
    int connfd=ca.connfd;
    struct sockaddr_in clientaddr=ca.clientaddr; 
    Free(vargp);

    rio_t rio_server, rio_client;
    char buf_server[MAXLINE],buf_client[MAXLINE];

//get the target client info
    Rio_readinitb(&rio_server, connfd);
    if (Rio_readlineb_w(&rio_server,buf_server,MAXLINE)==0){
        Close(connfd);
        return NULL;
    }

    char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    if (sscanf(buf_server, "%s %s %s",method, uri, version) != 3){
        printf("Request error");
        Close(connfd);
        return NULL;
    }

    char hostname[MAXLINE],pathname[MAXLINE],port[MAXLINE];
    if (parse_uri(uri,hostname,pathname,port) == -1){
        printf("URI error");
        Close(connfd);
        return NULL;
    }

//send the request to the client
P(&c);
    int clientfd=open_clientfd(hostname,port);
V(&c);
if (clientfd==-1){
	Close(connfd);
        return NULL;
}
    Rio_readinitb(&rio_client, clientfd);


    sprintf(buf_server, "%s /%s HTTP/1.1\r\n", method,pathname);
    Rio_writen_w(clientfd, buf_server, strlen(buf_server));
//Fputs(buf_server,debugfile);

    int n;
	int post_flag=0;
	int q_b_l;
    while ((n=Rio_readlineb_w(&rio_server,buf_server,MAXLINE))!=0){
        Rio_writen_w(clientfd, buf_server, n);
//Fputs(buf_server,debugfile);
		if (strncasecmp(buf_server,"Content-length:",15)==0){
            if (sscanf(buf_server+15,"%d",&q_b_l)!=1){ //get the length fo response body
//debugfile=Fopen("debug.txt","a");
//Fputs("requestHeaderLengthEOF\n",debugfile);
//Fclose(debugfile);
				Close(clientfd);
    			Close(connfd);
				return NULL;
			}
            post_flag=1;
            continue;
        }
        if (strcmp(buf_server,"\r\n")==0){  //request end
            break;
        }
    }
//Fclose(debugfile);
	if (n==0) {
//debugfile=Fopen("debug.txt","a");
//Fputs("requestHeaderEOF\n",debugfile);
//Fclose(debugfile);

		Close(clientfd);
		Close(connfd);
		return NULL;
	}
	if (post_flag){
		int seq=q_b_l;
                int i;
		for (i=0;i<seq;i++){
			n=Rio_readnb_w(&rio_server,buf_server,1);    
     
			if (n!=1) {

/*debugfile=Fopen("debug.txt","a");
Fputs("requestBodyEOF\n",debugfile);
sprintf(de,"%d\n",i);
Fputs(de,debugfile);
Fclose(debugfile);*/
				
				Close(clientfd);
				Close(connfd);
				return NULL;

			}
			
            Rio_writen_w(clientfd,buf_server,1);


        }
/*	int re=q_b_l%20;
        if (re!=0){
            n=Rio_readnb_w(&rio_server,buf_server,re);
    if (n==0) {
debugfile=Fopen("debug.txt","a");
Fputs("233EOF\n",debugfile);
Rio_writen_w(connfd,"\r\n",2);
Fclose(debugfile);
Close(clientfd);
  Close(connfd);
				return NULL;
			}
 debugfile=Fopen("debug.txt","a");
sprintf(de,"%d:%d\n",re,n);
Fputs(de,debugfile);
Fclose(debugfile);  
            Rio_writen_w(clientfd,buf_server,n);

		}*/

	}


//client receive the response and send it to server 
    size_t size=0;
    int r_b_l=0; //the length of the response body
    int flag=0; //whether response body exists or not

    while ((n=Rio_readlineb_w(&rio_client,buf_client,MAXLINE))!=0){
        Rio_writen_w(connfd,buf_client,n);
        size+=n;
/*debugfile=Fopen("debug.txt","a");
Fputs(buf_client,debugfile);
sprintf(de,"%ld\n",size);
Fputs(de,debugfile);
Fclose(debugfile); */ 


        if ( strncasecmp(buf_client,"Content-length:",15)==0){
            if (sscanf(buf_client+15,"%d",&r_b_l)!=1){
/*debugfile=Fopen("debug.txt","a");
Fputs("responseHeaderLengthEOF\n",debugfile);
Fclose(debugfile);*/
				Close(clientfd);
    			Close(connfd);
				return NULL;
			} //get the length fo response body
            flag=1;
            continue;
        }
        if (strcmp(buf_client,"\r\n")==0){
            break;
        }
    }

	if (n==0) {
/*debugfile=Fopen("debug.txt","a");
Fputs("responseHeaderEOF\n",debugfile);
Fclose(debugfile);*/

		Close(clientfd);
		Close(connfd);
		return NULL;
	}


 size+=r_b_l;
char logstring[MAXLINE];
    format_log_entry(logstring,&clientaddr,uri,size);

    P(&f);
    FILE *logfile=Fopen("proxy.log","a");
    Fputs(logstring,logfile);
    Fputs("\n",logfile);
    Fclose(logfile);
    printf("%s\n",logstring);
    fflush(stdout);
    V(&f);

    if (flag==1){
		if (r_b_l < 1000){
         int i;
        for (i=0;i<r_b_l/20;i++){
            n=Rio_readnb_w(&rio_client,buf_client,20);
			if (n==0) {
/*debugfile=Fopen("debug.txt","a");
Fputs("responseBodyEOF\n",debugfile);
Fclose(debugfile);*/
				Close(clientfd);
   				Close(connfd);
				return NULL;
			}
          //  buf_client[n]='\0';
 
            Rio_writen_w(connfd,buf_client,n);


        }
        if (r_b_l%20!=0){
            n=Rio_readnb_w(&rio_client,buf_client,r_b_l%20);
			if (n==0) {
/*debugfile=Fopen("debug.txt","a");
Fputs("responseBodyEOF\n",debugfile);
Fclose(debugfile);*/
				Close(clientfd);
    			Close(connfd);
				return NULL;
			}
        //    buf_client[n]='\0';

            Rio_writen_w(connfd,buf_client,n);


}
        Rio_writen_w(connfd,"\r\n",2);


       }
		else{

			int len=r_b_l;


			while(len>0){
				n=Rio_readnb_w(&rio_client,buf_client,1);
				if (n==0) {
/*debugfile=Fopen("debug.txt","a");
Fputs("responseBodyEOF\n",debugfile);
Fclose(debugfile);*/
 					Close(clientfd);
    				Close(connfd);
					return NULL;
				}
				Rio_writen_w(connfd,buf_client,n);
				len--;
//buf_client[n]='\0';

			}
			Rio_writen_w(connfd,"\r\n",2);
		}

}

    Close(clientfd);

    

    Close(connfd);

    return NULL;
}

/*
 * Rio wrappers - simply return after printing a warning message 
 *                  when I/O fails rather than terminate the process
 */
ssize_t Rio_readn_w(int fd, void *ptr, size_t nbytes){
    ssize_t n;
  
    if ((n = rio_readn(fd, ptr, nbytes)) < 0){
        printf("Rio_readn error");
        return 0;
    }
    return n;
}

ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen){
    ssize_t rc;

    if ((rc = rio_readlineb(rp, usrbuf, maxlen)) < 0){
        printf("Rio_readlineb error");
        return 0;
    }
    return rc;
}

ssize_t Rio_readnb_w(rio_t *rp, void *usrbuf, size_t n){
    ssize_t rc;

    if ((rc = rio_readnb(rp, usrbuf, n)) < 0){
        printf("Rio_readnb error");
        return 0;
    }
    return rc;
}

void Rio_writen_w(int fd, void *usrbuf, size_t n){
    if (rio_writen(fd, usrbuf, n) != n)
        printf("Rio_writen error");
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
 * (sockaddr), the URI from the request (uri), the number of bytes
 * from the server (size).
 */
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


