#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <unistd.h>
#include <ucontext.h>

#define PORT 8080

//typedef struct psu_thread_info psu_thread_info_t;

// the thread that will run
pthread_t thread_id;

// the user context. client: saves into this and sends. server: saves into this
// TODO: maybe not this? maybe some other structure?
ucontext_t uctx;

int server_mode;



// receive the context from client
void server()
{
	int portno = PORT;
	int sockfd, newsockfd;
	struct sockaddr_in serv_addr, cli_addr;
	socklen_t clilen;
	char buffer[256];
	int n;

	// start a server
	// create a socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) error("ERROR opening socket");

	// set the buffer to zero
	bzero((char *) &serv_addr, sizeof(serv_addr));

	// set my attributes in serv_add. This is me
	// my own address: INADDR_ANY
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);

	// bind myself (the server) with the socket
	// clients will try to communicate using this
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
		error("ERROR on binding");

	// listen for connections
	printf("Listening for connections.\n");
	listen(sockfd,5);
	clilen = sizeof(cli_addr);

	// accept the connection. all future communications with the client:
	// needs to be done through this new socket_descriptor
	newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
	if (newsockfd < 0) 
	  error("ERROR on accept");
	bzero(buffer,256);

	// receive the context from the client
	// TODO: maybe not this structure exactly??
	n = read(newsockfd, &uctx, sizeof(uctx));
	if (n < 0) error("ERROR reading from socket");

	// simple check if got the same value as sender
	printf("%d\n", uctx.uc_stack.ss_sp);

	// write to the client
	n = write(newsockfd,"I got your message, I'll start your thread!", 43);
	if (n < 0) error("ERROR writing to socket");

	// close the server
	close(newsockfd);
	close(sockfd);
}



// send the context saved in uctx to the remote server
void client(const char * hostname)
{
	int sockfd, n, portno = PORT;
	struct sockaddr_in serv_addr;
	struct hostent *server;
	char buffer[256];

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) 
		error("ERROR opening socket");

	// try to connect. if failure, return failure msg
	server = gethostbyname(hostname);

	if (server == NULL) {
		fprintf(stderr,"ERROR, no such host\n");
		exit(0);
	}

	// set attributes for the server. set internet property, then
	// byte-wise copy server's address, and set the port number
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
	serv_addr.sin_port = htons(portno);

	// connect to the server by placing it in the other end of the socket
	if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
		error("ERROR connecting");

	printf("%d\n", uctx.uc_stack.ss_sp);

	// write the context to server
	// TODO: something else to be written here?
	n = write(sockfd, &uctx, sizeof(uctx));
	if (n < 0) 
		error("ERROR writing to socket");
	bzero(buffer, 256);

	// read from the server, and print
	n = read(sockfd, buffer, 255);
	if (n < 0) 
		error("ERROR reading from socket");
	printf("%s\n",buffer);
	
	// close the socket
	close(sockfd);
}



void psu_thread_setup_init(int mode)
{
	server_mode = mode;
	if (server_mode) {
		server();
		// switch to the new context: uctx
		// TODO: how?? this does not  make sense in my machine!
	} else {
		// This is a machine where the thread starts first
		// TODO: do any initializations here?? not sure...
		
	}
	return;
}

int psu_thread_create(void * (*user_func)(void*), void *user_args)
{
	int iret = pthread_create( &thread_id, NULL, user_func, user_args);
	pthread_join (thread_id, NULL);
	return iret;
}

void psu_thread_migrate(const char *hostname)
{
	const int retval = 0;
	getcontext(&uctx);

	if (!server_mode) {
		// start a client right here. 
		// using this, send the context
		client(hostname);

		// if success, then exit this thread
		pthread_exit(&retval);

	} else {
		// I am a server. simply continue
	}

	return;
}
