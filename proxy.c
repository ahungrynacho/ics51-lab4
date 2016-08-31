//gcc csapp.c proxy.c -o <name of executable> -pthread
// Brian Huynh 57641580
// Joanne Trinh 87597916

#include "csapp.h"
#define PROXY_LOG "proxy.log"
#define DEBUG

typedef struct 
{
    int myid;   
    int connfd;                 
    struct sockaddr_in clientaddr; 
} arglist_t;


FILE *log_file; 
sem_t mutex;    
int parseURI(char *uri, char *addressTarget, char *path, int  *port);
void logentryFormat(char *logstring, struct sockaddr_in *socketAddress, char *uri, int size);
void *handleRequest(void* varGP);
int open_clientfd_ts(char *hostname, int port, sem_t *mutexp); 
ssize_t Rio_readn_w(int forward, void *pointer, size_t numBytes);
ssize_t Rio_readlineb_w(rio_t *rpointer, void *userBuff, size_t maxLength); 
void Rio_writen_w(int forward, void *userBuff, size_t n);


void *handleRequest(void *varGP) 
{
    arglist_t argList;              
    struct sockaddr_in clientaddr;        
    int connfd, serverfd, i, n, responseLength, requestLength, reallocFactor, port;
    char *request, *requestURI, *requestURIend, *restofRequest, hostname[MAXLINE], pathname[MAXLINE], logEntry[MAXLINE], buffer[MAXLINE];        

    rio_t rio;                      

    argList = *((arglist_t *)varGP); 
    connfd = argList.connfd;          
    clientaddr = argList.clientaddr;
    Pthread_detach(pthread_self());  
    Free(varGP);                     

    request = (char *)Malloc(MAXLINE);
    request[0] = '\0';
    reallocFactor = 2;
    requestLength = 0;
    Rio_readinitb(&rio, connfd);
    
    while (1)
    {
    	if ((n = Rio_readlineb_w(&rio, buffer, MAXLINE)) <= 0) 
    	{
    	    printf("Client issued a bad request.\n");
    	    close(connfd);
    	    free(request);
    	    return NULL;
    	}
    	
    	if (requestLength + n + 1 > MAXLINE)
    	    Realloc(request, MAXLINE*reallocFactor++);

    	strcat(request, buffer);
    	requestLength += n;

    	if (strcmp(buffer, "\r\n") == 0)
    	    break;
    }
    
#if defined(DEBUG) 	
    {
	struct hostent *hostPointer;
	char *hostAddress;
	
	P(&mutex);
	hostPointer = Gethostbyaddr((char *)&clientaddr.sin_addr.s_addr,
			   sizeof(clientaddr.sin_addr.s_addr), 
			   AF_INET);
	hostAddress = inet_ntoa(clientaddr.sin_addr);
	printf("Thread %d: Received request from %s (%s):\n", argList.myid,
	       hostPointer->h_name, hostAddress);
	printf("%s", request);
	//printf("*** End of Request ***\n");
	printf("\n");
	fflush(stdout);
	V(&mutex);
    }
#endif

    
    if (strncmp(request, "GET ", strlen("GET ")))
    {
    	printf("Received non-GET request\n");
    	close(connfd);
    	free(request);
    	return NULL;
    }
    
    requestURI = request + 4;

   
    requestURIend = NULL;
    for (i = 0; i < requestLength; i++) 
    {
    	if (requestURI[i] == ' ')
    	{
    	    requestURI[i] = '\0';
    	    requestURIend = &requestURI[i];
    	    break;
    	}
    }

    if ( i == requestLength ) 
    {
    	printf("handleRequest: Couldn't find the end of the URI\n");
    	close(connfd);
    	free(request);
    	return NULL;
    }

    if (strncmp(requestURIend + 1, "HTTP/1.0\r\n", strlen("HTTP/1.0\r\n")) &&
	strncmp(requestURIend + 1, "HTTP/1.1\r\n", strlen("HTTP/1.1\r\n"))) 
	{
    	printf("Client issued a bad request.\n");
    	close(connfd);
    	free(request);
    	return NULL;
    }

    restofRequest = requestURIend + strlen("HTTP/1.0\r\n") + 1;

    if (parseURI(requestURI, hostname, pathname, &port) < 0) 
    {
    	printf("Cannot parse uri.\n");
    	close(connfd);
    	free(request);
    	return NULL;
    }    

    if ((serverfd = open_clientfd_ts(hostname, port, &mutex)) < 0)
    {
    	printf("Unable to connect to end server.\n");
    	free(request);
    	return NULL;
    }
    
    Rio_writen_w(serverfd, "GET /", strlen("GET /"));
    Rio_writen_w(serverfd, pathname, strlen(pathname));
    Rio_writen_w(serverfd, " HTTP/1.0\r\n", strlen(" HTTP/1.0\r\n"));
    Rio_writen_w(serverfd, restofRequest, strlen(restofRequest));
  
#if defined(DEBUG) 	
    P(&mutex);
    printf("Thread %d: Forwarding request to end server:\n", argList.myid);
    printf("GET /%s HTTP/1.0\r\n%s", pathname, restofRequest);
    //printf("*** End of Request ***\n");
    printf("\n");
    fflush(stdout);
    V(&mutex);
#endif

    Rio_readinitb(&rio, serverfd);
    responseLength = 0;
    
    while( (n = Rio_readn_w(serverfd, buffer, MAXLINE)) > 0 ) 
    {
    	responseLength += n;
    	Rio_writen_w(connfd, buffer, n);
    #if defined(DEBUG)	
    	printf("Thread %d: Forwarded %d bytes from end server to client\n", argList.myid, n);
    	fflush(stdout);
    #endif
    	bzero(buffer, MAXLINE);
    }

    logentryFormat(logEntry, &clientaddr, requestURI, responseLength);  
    P(&mutex);
    fprintf(log_file, "%s %d\n", logEntry, responseLength);
    fflush(log_file);
    V(&mutex);

    close(connfd);
    close(serverfd);
    free(request);
    return NULL;
}

ssize_t Rio_readn_w(int forward, void *pointer, size_t numBytes) 
{
    ssize_t n;
  
    if ((n = rio_readn(forward, pointer, numBytes)) < 0)
    {
    	printf("Rio_readn failed\n");
    	return 0;
    }    
    return n;
}

ssize_t Rio_readlineb_w(rio_t *rpointer, void *userBuff, size_t maxLength) 
{
    ssize_t rc;

    if ((rc = rio_readlineb(rpointer, userBuff, maxLength)) < 0) 
    {
    	printf("Rio_readlineb failed\n");
    	return 0;
    }
    return rc;
} 

void Rio_writen_w(int forward, void *userBuff, size_t n) 
{
    if (rio_writen(forward, userBuff, n) != n) 
    {
	    printf("Rio_writen failed.\n");
    }	   
}


int open_clientfd_ts(char *hostname, int port, sem_t *mutexp) 
{
    int clientfd;
    struct hostent hostent, *hostPointer = &hostent;
    struct hostent *temp_hp;
    struct sockaddr_in serveraddr;

    if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	return -1; 

    P(mutexp); 
    temp_hp = gethostbyname(hostname);
    if (temp_hp != NULL)
	hostent = *temp_hp; 
    V(mutexp);
   
    if (temp_hp == NULL)
	return -2; 
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)hostPointer->h_addr, 
	  (char *)&serveraddr.sin_addr.s_addr, hostPointer->h_length);
    serveraddr.sin_port = htons(port);

    if (connect(clientfd, (SA *) &serveraddr, sizeof(serveraddr)) < 0)
	return -1;
    return clientfd;
}

int parseURI(char *uri, char *hostname, char *pathname, int *port)
{
    char *hostbegin, *hostend, *pathbegin;
    int len;

    if (strncasecmp(uri, "http://", 7) != 0)
    {
    	hostname[0] = '\0';
    	return -1;
    }
 
    hostbegin = uri + 7;
    hostend = strpbrk(hostbegin, " :/\r\n\0");
    len = hostend - hostbegin;
    strncpy(hostname, hostbegin, len);
    hostname[len] = '\0';
    
    *port = 80; /* default */
    if (*hostend == ':')   
	*port = atoi(hostend + 1);
    
    pathbegin = strchr(hostbegin, '/');
    if (pathbegin == NULL)
    {
    	pathname[0] = '\0';
    }
    else
    {
    	pathbegin++;	
    	strcpy(pathname, pathbegin);
    }

    return 0;
}

void logentryFormat(char *logstring, struct sockaddr_in *socketAddress, char *uri, int size)
{
    time_t now;
    char time_str[MAXLINE];
    unsigned long host;
    unsigned char a, b, c, d;

    now = time(NULL);
    strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));
    
    host = ntohl(socketAddress->sin_addr.s_addr);
    a = host >> 24;
    b = (host >> 16) & 0xff;
    c = (host >> 8) & 0xff;
    d = host & 0xff;

    sprintf(logstring, "%s: %d.%d.%d.%d %s", time_str, a, b, c, d, uri);
}

int main(int argc, char **argv)
{
    int listenfd;             
    int port;                 
    pthread_t tid;            
    int clientlen;            
    arglist_t *argp = NULL;  
    int request_count = 0;    
  
    if (argc != 2)
    {
    	fprintf(stderr, "port number.\n", argv[0]);
    	exit(0);
    }

    signal(SIGPIPE, SIG_IGN);

    port = atoi(argv[1]);
    listenfd = Open_listenfd(argv[1]);

    log_file = Fopen(PROXY_LOG, "a");
    Sem_init(&mutex, 0, 1); 
 
    while (1) 
    { 
    	argp = (arglist_t *)Malloc(sizeof(arglist_t));
    	clientlen = sizeof(argp->clientaddr);
    	argp->connfd = 
    	  Accept(listenfd, (SA *)&argp->clientaddr, (socklen_t *) &clientlen); /*IGH*/
    
    	argp->myid = request_count++;
    	pthread_create(&tid, NULL, handleRequest, argp);
    }

    exit(0);
}