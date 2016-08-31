
///////////////////////
//      HEADERS      //
///////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "csapp.c"

int DEBUG = 1;
#define LOG_FILE "proxy.log"

/* 
 * This struct remembers some key attributes of an HTTP request and
 * the thread that is processing it.
 */
typedef struct {
    int myid;    /* Small integer used to identify threads in debug messages */
    int connfd;                    /* Connected file descriptor */ 
    struct sockaddr_in clientaddr; /* Client IP address */
} arglist_t;


int parse_uri(char *uri, char *hostname, char *pathname, int *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size);
void debug_notify(char *s);


///////////////////////
//       Main        //
///////////////////////


int main(int argc, char *argv[]){
  if(argc == 1){
    printf("%s\n", "Please specify a port.");
  }
  else if(atoi(argv[1]) < 1024 || atoi(argv[1]) > 65536){
    printf("%s\n", "Invalid port");
  }
  else{

    int port = atoi(argv[1]);
    int socket = open_listenfd(port);


    if(socket == -1){
      printf("%s\n", "Port error");
    }
    else{
      FILE *proxy_log = fopen(LOG_FILE, "a");
      while(1){

        struct sockaddr_in server, client;
        int clientlen = sizeof(struct sockaddr_in);
        arglist_t *argp = (arglist_t *)Malloc(sizeof(arglist_t));
        argp->connfd = accept(socket, (struct sockaddr *)&client, (socklen_t*)&clientlen);

        struct sockaddr_in clientaddr = argp->clientaddr;

        // server.sin_addr.s_addr = inet_addr("127.0.0.1");
        // server.sin_family = AF_INET;
        // server.sin_port = htons( 8888 );

        // int client_sock = open_clientfd("localhost",8889);
        
        char *request = (char * )malloc(MAXLINE);
        char buf[MAXLINE];
        int request_len = 0;
        int realloc_factor = 2;
        int n = 0;
        rio_t rio;
        debug_notify("Start reading");
        Rio_readinitb(&rio, argp->connfd);

        int i = 1;
        while(1){
          if((n = rio_readlineb(&rio, buf, MAXLINE  )) <= 0){
            return -1;
          }
          debug_notify(buf);

          if(request_len + n + 1 > MAXLINE)
            request = realloc(request, MAXLINE*realloc_factor++);

          strcat(request, buf);
          request_len += n;

          if(strcmp(buf, "\r\n") == 0)
            break;
        }

        // Request URI begins after "GET "
        char *request_uri = request + 4;

        /* 
         * Extract the URI from the request
         */
        char *request_uri_end = NULL;
        for (i = 0; i < request_len; i++) {
          if (request_uri[i] == ' ') {
              request_uri[i] = '\0';
              request_uri_end = &request_uri[i];
              break;
          }
        }

        char *rest  = request_uri_end + strlen("HTTP/1.0\r\n") + 1;;


        debug_notify("NOW WRITE");
        Rio_writen(argp->connfd, "Hello there!", 13);
        debug_notify("NOW EXIT");

        char log_entry[MAXLINE];
        format_log_entry(log_entry, &argp->clientaddr, request_uri, 13);
        debug_notify(log_entry);
        fprintf(proxy_log, "%s\n", log_entry);
        fflush(proxy_log);
        close(argp->connfd);
      } 
    }
  }

  return 0;
}






////////////////////
//     Helpers    //
////////////////////


/*
 * parse_uri - URI parser
 * 
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int parse_uri(char *uri, char *hostname, char *pathname, int *port)
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
    len = hostend - hostbegin;
    strncpy(hostname, hostbegin, len);
    hostname[len] = '\0';
    
    /* Extract the port number */
    *port = 80; /* default */
    if (*hostend == ':')   
  *port = atoi(hostend + 1);
    
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

void debug_notify(char *s){
  if(DEBUG){printf("%s\n", s);}
}