/*
 * proxy.c - CS:APP Web proxy
 *
 * Student ID: 2016-81755
 *         Name: Kozar Anastasiia
 * 
 * So proxy at the begining act as server by accepting clients and
 * creating thread to serve them. Then it starts act as client, by 
 * connecting to server and pass there a message. Then it reads 
 * message back and writes it in client, taking a log at the same time.
 */ 

#include "csapp.h"

/* The name of the proxy's log file */
#define PROXY_LOG "proxy.log"
#define MAXTIMESIZE 1024
/* Undefine this if you don't want debugging output */
#define DEBUG

static FILE *file;

static sem_t send_mutex;
static sem_t file_mutex;

typedef struct {
    int connfd;
    char* host;
    int port;
}client_s;
/*
 * Functions to define
 */
void *process_request(void* vargp);
int open_clientfd_ts(char *hostname, int port, sem_t *mutexp);
ssize_t Rio_readn_w(int fd, void *ptr, size_t nbytes);
ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen);
void Rio_writen_w(int fd, void *usrbuf, size_t n);

/*
 * main - Main routine for the proxy program
 */
int main(int argc, char **argv)
{
    int port = atoi(argv[1]);
    struct sockaddr_in clientaddr;
    int clientlen=sizeof(clientaddr);
    pthread_t tid; 
    client_s *client;
    
    /* Check arguments */
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
        exit(0);
    } 

    /*Ignoring pipe signals*/
    Signal(SIGPIPE, SIG_IGN);
    int listenfd = Open_listenfd(port);
    
    /*some semaphors for thread safety*/
    Sem_init(&send_mutex, 0 , 1);
    Sem_init(&file_mutex, 0 , 1);

    //file = Fopen("/home/deve1/proxylab-handout/proxy.log", "w");
    /*accepting clients*/
    while(1) {
	struct hostent *hp;
	client = Malloc(sizeof(client));
	client->connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);
	/* determine the domain name and IP address of the client */
	hp = Gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
			   sizeof(clientaddr.sin_addr.s_addr), AF_INET);
	/*fill in the info about client*/
	client->host = inet_ntoa(clientaddr.sin_addr);
	client->port = ntohs(clientaddr.sin_port);
	printf("Proxy server connected to %s (%s), port %d at connfd %d\n", 
	    hp->h_name, client->host, client->port, client->connfd);
	Pthread_create(&tid, NULL, process_request, (void*) client);
        //pthread_detach(tid);
    }

    Close(listenfd);
    exit(0);
}

void *process_request(void* vargp) {
    int clientfd, port, n, flag = 0;
    char *host;
    char buf[MAXLINE], message[MAXLINE];
    char log[MAXLINE], *args, *argport, input[MAXLINE];
    rio_t rio, rios;
    time_t now;
    struct tm *timeinfo;
    char prefix[40];
    pthread_t tid = pthread_self();
    
    client_s* client;
    client = (client_s*)vargp;
    
    Pthread_detach(pthread_self()); 
    printf("Served by thread %ul\n", tid);

    Rio_readinitb(&rio, client->connfd);

    /*proxy becomes a client of echoserver*/
    while((n = Rio_readlineb_w(&rio, &buf, MAXLINE)) != 0) {
	
        /*parse line*/
	strcpy(input, buf);
	if(!(host = strtok_r(input, " ", &args)))
		flag = 1;
	if(!(argport = strtok_r(NULL, " ", &args)))
		flag = 2;
	strcpy(message, args);
	if(flag != 0) {
		sprintf(buf, "proxe usage: <host> <port number> <message>\n");
		Rio_writen_w(client->connfd, buf, strlen(buf));
		flag = 0;
		continue;
	}
	port = atoi(argport);
	printf("%sReceived %s", prefix, buf);
	/*open connection to server*/
	if((clientfd = open_clientfd_ts(host, port, &send_mutex)) < 0) {
		fprintf(stderr, "Cannot connect to: %s, %d\n", host, port);
		continue;
	}
	/*dialog with server*/
	Rio_readinitb(&rios, clientfd);
    	Rio_writen_w(clientfd, message, strlen(message));
	Rio_readlineb_w(&rios, buf, MAXLINE);
	
	/*write back to echoclient*/
	Rio_writen_w(client->connfd, buf, strlen(buf));
	Close(clientfd);

	/*log*/
	time(&now);
	timeinfo = localtime(&now);
	strftime(log, MAXTIMESIZE, "%a %d %b %Y %H %M: %S %Z", timeinfo);
	P(&file_mutex); /*locking while writing, because it's dangerous*/
    	/*open file for log*/
    	file = Fopen(PROXY_LOG, "a");
	fprintf(file, "%s: %s %d %d %s", log, client->host, client->port, strlen(buf), buf);
	/*close all that must be closed*/
   	Fclose(file);
	V(&file_mutex); /*unlocking*/

    }
    sprintf(prefix, "Thread %u ", (unsigned int) tid);
    Close(client->connfd);
    Free(vargp);
    return NULL;
}

/*creates thread safe connection from proxy to server*/
int open_clientfd_ts(char *hostname, int port, sem_t *mutexp) {
    int clientfd; 
    struct hostent *hp; 
    struct sockaddr_in serveraddr; 
    if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
	return -1; /* check errno for cause of error */ 

    /* Fill in the server's IP address and port */ 
    P(mutexp); /*locking before connection to host*/
    if ((hp = gethostbyname(hostname)) == NULL) 
	return -2; /* check h_errno for cause of error */ 
    V(mutexp); /*unloking after reading*/
    
    /*Initialization of serveraddr*/
    bzero((char *) &serveraddr, sizeof(serveraddr)); 
    serveraddr.sin_family = AF_INET; 
    bcopy((char *)hp->h_addr_list[0], (char *)&serveraddr.sin_addr.s_addr, hp->h_length); 
    serveraddr.sin_port = htons(port); 

    /* Establish a connection with the server */ 
    if (connect(clientfd, (SA *) &serveraddr, sizeof(serveraddr)) < 0) 
	return -1; 
    return clientfd; 
}

/*wrapper for rio_readn*/
ssize_t Rio_readn_w(int fd, void *ptr, size_t nbytes) {
    ssize_t n;
    if((n = rio_readn(fd, ptr, nbytes)) < 0) {
	printf("rio_readn error\n");
	return 0;
    }
    return n;
}

/*wrapper for rio_readlineb*/
ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen) {
    ssize_t n;
    if((n = rio_readlineb(rp, usrbuf, maxlen)) < 0) {
	printf("rio_readlineb error\n");
	return 0;
    }
    return n;
}

/*wrapper for rio_writen*/
void Rio_writen_w(int fd, void *usrbuf, size_t n) {
    if(rio_writen(fd, usrbuf, n) != n) {
	printf("rio_writen error\n");
	return;
    }
}
