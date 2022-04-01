#include <pthread.h>
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

#define EIP 14
#define EBP 6
#define ESP 7
#define EAX 11

#define STACK_WIDTH 4

#define PORT 8080

#define NUM_BLANK_ENTRIES 1400
#define SIZE_OF_STACK 2000
#define MAX_RECURSION_DEPTH 20
#define MAX_ARGS 20

//
// for app3. hold base pointer pthread_create
//
uint32_t base_pointer_create;
//

typedef struct psu_thread_info {
	
	ucontext_t uctx;
	uint32_t stack_raw_data[SIZE_OF_STACK];
	//uint32_t args[100];
	uint32_t restart_point;
	int num_elements;
	int restart_point_offset;
	int num_blank_entries;
	int base_pointer_positions[MAX_RECURSION_DEPTH];
	int num_frames;
	
} psu_thread_info_t;

psu_thread_info_t psuthreadinfo;
uint32_t fn_start_addr;
pthread_t thread_id;

// the user context. client: saves into this and sends. server: saves into this
// TODO: maybe not this? maybe some other structure?

int server_mode;

void error(char *s){
	puts(s);
	exit(-1);
}

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
	
	int option = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

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
	// printf("Listening for connections.\n");
	listen(sockfd,5);
	clilen = sizeof(cli_addr);

	// accept the connection. all future communications with the client:
	// needs to be done through this new socket_descriptor
	newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
	if (newsockfd < 0) 
	  error("ERROR on accept");

	// receive the context from the client
	// TODO: maybe not this structure exactly??
	int bytes_to_read = sizeof(psuthreadinfo);
	char * ptr = &psuthreadinfo;
	while (bytes_to_read > 0) {
		n = read(newsockfd, ptr, bytes_to_read);
		if (n < 0) {
			error ("ERROR reading from socket");
		}
		ptr = ptr + n;
		bytes_to_read -= n;
	}
	if (bytes_to_read == 0) {
		//printf("Have received all the bytes!\n");
		;
	}

	// write to the client
	bzero(buffer, 256);
	n = write(newsockfd,"I got your message, I'll start your thread!", 43);
	if (n < 0) error("ERROR writing to socket");
	
	//printf("I received: num_blank_entries: %d\n", psuthreadinfo.num_blank_entries);
	//printf("EAX: %zx\n", psuthreadinfo.uctx.uc_mcontext.gregs[EAX]);
    
	//printf("closing server\n");
	// close the server
	close(newsockfd);
	close(sockfd);
    //printf("server closed\n");
	
	
	ucontext_t uctx_local;
    getcontext(&uctx_local);
	uint32_t *ebp = uctx_local.uc_mcontext.gregs[EBP];
    uint32_t *ebp_last_frame = *ebp;
    uint32_t  ret_addr = *(ebp+1);
	uint32_t restart_addr = psuthreadinfo.restart_point_offset + fn_start_addr;
	
	int i;
	for (i = 0; i < psuthreadinfo.num_frames-1; i++) {
		psuthreadinfo.stack_raw_data[psuthreadinfo.base_pointer_positions[i]] = &psuthreadinfo.stack_raw_data[psuthreadinfo.base_pointer_positions[i+1]];
	}
	psuthreadinfo.stack_raw_data[psuthreadinfo.base_pointer_positions[psuthreadinfo.num_frames-1]] = ebp_last_frame;
	psuthreadinfo.stack_raw_data[psuthreadinfo.base_pointer_positions[psuthreadinfo.num_frames-1]+1] = ret_addr;
	
	psuthreadinfo.uctx.uc_mcontext.gregs[ESP] = &psuthreadinfo.stack_raw_data[psuthreadinfo.num_blank_entries];
    psuthreadinfo.uctx.uc_mcontext.gregs[EBP] = &psuthreadinfo.stack_raw_data[psuthreadinfo.base_pointer_positions[0]];
    psuthreadinfo.uctx.uc_mcontext.gregs[EIP] = restart_addr;
	
	setcontext(&psuthreadinfo.uctx);
	
	
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

	// write the context to server
	// TODO: something else to be written here?
	int bytes_to_send = sizeof(psuthreadinfo);
	char * ptr = &psuthreadinfo;
	while (bytes_to_send > 0) {
		n = write(sockfd, ptr, bytes_to_send);
		if (n < 0) {
			error ("ERROR writing to socket");
		}
		ptr = ptr + n;
		bytes_to_send -= n;
	}
	if (bytes_to_send == 0) {
		//printf("Sent all the bytes!\n");
		;
	}
	
	bzero(buffer, 256);

	// read from the server, and print
	n = read(sockfd, buffer, 255);
	if (n < 0) 
		error("ERROR reading from socket");
	//printf("%s\n",buffer);
	
	// close the socket
	close(sockfd);
	
	
}



void psu_thread_setup_init(int mode)
{
	psuthreadinfo.num_blank_entries = NUM_BLANK_ENTRIES;
	server_mode = mode;
	return;
}


void * server_wrapper(void * arg) {
	
	server();
	
	int i = 0;
	pthread_exit(&i);
	return NULL;
}


int psu_thread_create(void * (*user_func)(void*), void *user_args)
{
	fn_start_addr = user_func;
	int iret;
	
	ucontext_t uctx_local;
	getcontext(&uctx_local);
	base_pointer_create = psuthreadinfo.uctx.uc_mcontext.gregs[EBP];
    
	if (server_mode){
		iret = pthread_create( &thread_id, NULL, server_wrapper, user_args);
		pthread_join (thread_id, NULL);
		return iret;
	} else {
		iret = pthread_create( &thread_id, NULL, user_func, user_args);
		pthread_join (thread_id, NULL);
		return iret;
	}
	
}

void psu_thread_migrate(const char *hostname)
{
	int i;
	const int retval = 0;
    //printf("entered migrate\n");
	getcontext(&(psuthreadinfo.uctx));
	
	uint32_t *ebp = psuthreadinfo.uctx.uc_mcontext.gregs[EBP];
    uint32_t *ebp_last_frame = *ebp;
    uint32_t *esp = psuthreadinfo.uctx.uc_mcontext.gregs[ESP];
    uint32_t *esp_last_frame = ebp+2;
    uint32_t  ret_addr = *(ebp+1);
    psuthreadinfo.restart_point = ret_addr;
    psuthreadinfo.restart_point_offset = psuthreadinfo.restart_point - fn_start_addr;
	
	//printf("Print the base pointers and their contents\n");
	uint32_t bp_cur;
	uint32_t bp_arr[MAX_RECURSION_DEPTH+2];
	psuthreadinfo.num_frames = 0;
	for (bp_cur = (uint32_t)ebp; bp_cur != base_pointer_create; bp_cur = *((uint32_t*)bp_cur)) {
		//printf("Address: %zx has content: %zx\n", bp_cur, *((uint32_t*)bp_cur));
		bp_arr[psuthreadinfo.num_frames++] = *((uint32_t*)bp_cur);
	}
	psuthreadinfo.num_frames -= 2;
	//printf("Number of frames we are interested in: %d\n", psuthreadinfo.num_frames);
	//printf("The bps that we are interested in: \n");
	for (i = 0; i < psuthreadinfo.num_frames; i++) {
		//printf("%zx\n", bp_arr[i]);
		psuthreadinfo.base_pointer_positions[i] = psuthreadinfo.num_blank_entries + ((unsigned long int)bp_arr[i] - (unsigned long int)esp_last_frame)/STACK_WIDTH;
	}
	
	uint32_t * ebp_deepest_frame = bp_arr[psuthreadinfo.num_frames-1];
	//printf("The deepest frame's bp is: %zx\n", ebp_deepest_frame );

    for (i = 0; i < ((unsigned long int)ebp_deepest_frame-(unsigned long int)esp_last_frame)/(STACK_WIDTH)+1; i++)
    {
        psuthreadinfo.stack_raw_data[psuthreadinfo.num_blank_entries+i] = *(esp_last_frame+i);
        psuthreadinfo.num_elements = i;
    }
	//printf("Number of elements in stack: %d\n", psuthreadinfo.num_elements);
	
    for (i = 0; i < MAX_ARGS; i++)
    {
        psuthreadinfo.stack_raw_data[psuthreadinfo.num_blank_entries+psuthreadinfo.num_elements+1+i] = *(ebp_deepest_frame+1+i);
    }
	
	//printf("The bp's accessed using array of bp positions:\n");
	for (i = 0; i < psuthreadinfo.num_frames; i++) {
		//printf("Index: %d, value: %zx\n", psuthreadinfo.base_pointer_positions[i], psuthreadinfo.stack_raw_data[psuthreadinfo.base_pointer_positions[i]]);
	}
	
	// start a client right here. 
	// using this, send the context
	// store all info needed for that
	//printf("migration starting\n");
	client(hostname); //sending uctx insinde of this function
	
	pthread_exit(&retval);
	
	return;
}