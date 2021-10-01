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

#define PORT 8081

#define NUM_BLANK_ENTRIES 1400
#define SIZE_OF_STACK 1500
#define NUM_FRAMES 3

typedef struct psu_thread_info {
	
	ucontext_t uctx;
	uint32_t stack_raw_data[SIZE_OF_STACK];
	uint32_t args[100];
	uint32_t restart_point;
	int num_elements;
	int restart_point_offset;
	int num_blank_entries;
    int frame_index[NUM_FRAMES];
	
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
	printf("Listening for connections.\n");
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
		printf("Have received all the bytes!\n");
	}

	// write to the client
	bzero(buffer, 256);
	n = write(newsockfd,"I got your message, I'll start your thread!", 43);
	if (n < 0) error("ERROR writing to socket");
	
	printf("I received: num_blank_entries: %d\n", psuthreadinfo.num_blank_entries);
	printf("EAX: %zx\n", psuthreadinfo.uctx.uc_mcontext.gregs[EAX]);
    
	printf("closing server\n");
	// close the server
	close(newsockfd);
	close(sockfd);
    printf("server closed\n");
	
	
	ucontext_t uctx_local;
	
    getcontext(&uctx_local);

	uint32_t *ebp = uctx_local.uc_mcontext.gregs[EBP];
    uint32_t *ebp_last_frame = *ebp;
    uint32_t  ret_addr = *(ebp+1);
	uint32_t restart_addr = psuthreadinfo.restart_point_offset + fn_start_addr;
	
	psuthreadinfo.uctx.uc_mcontext.gregs[ESP] = &psuthreadinfo.stack_raw_data[psuthreadinfo.frame_index[psuthreadinfo.num_blank_entries]];
	
    psuthreadinfo.uctx.uc_mcontext.gregs[EBP] = &psuthreadinfo.stack_raw_data[psuthreadinfo.num_blank_entries + psuthreadinfo.num_elements];
    for(int k = NUM_FRAMES-1; k>=0; k--){//link ebps together 
        //psuthreadinfo.stack_raw_data[psuthreadinfo.num_blank_entries + psuthreadinfo.num_elements] = ebp_last_frame;//
        psuthreadinfo.stack_raw_data[psuthreadinfo.frame_index[k]] = ebp_last_frame;//
        if (k != -1){ //gives segfault on last iteration
            printf("%d",k-1);
        
            ebp_last_frame = psuthreadinfo.frame_index[k-1];
        }
    }

    //psuthreadinfo.stack_raw_data[psuthreadinfo.num_blank_entries+psuthreadinfo.num_elements+1] = ret_addr;
    psuthreadinfo.stack_raw_data[psuthreadinfo.frame_index[NUM_FRAMES-1] + 1] = ret_addr;
    psuthreadinfo.uctx.uc_mcontext.gregs[EIP] = restart_addr;
	printf("setting context\n");
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
		printf("Sent all the bytes!\n");
	}
	
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
	psuthreadinfo.num_blank_entries = NUM_BLANK_ENTRIES;
	server_mode = mode;
	return;
}


void * server_wrapper(void * arg) {
	server();
	int ret = 0;
	int x = 10;
	x++;
	int y = x;
	y++;
	pthread_exit(&ret);
	return NULL;
}


int psu_thread_create(void * (*user_func)(void*), void *user_args)
{
	fn_start_addr = user_func;
	int iret;
	
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
    printf("entered migrate\n");
    if (server_mode){
        printf("I am a server\n--------\n");
        //pthread_exit(&retval);
        return;
    }

	getcontext(&(psuthreadinfo.uctx));
	
    uint32_t *ebp = psuthreadinfo.uctx.uc_mcontext.gregs[EBP];
    uint32_t *ebp_last_frame = *ebp;
    //printf("\nebp %lu\n", ebp);
    //printf("*ebp %lu\n", *ebp);
    uint32_t *esp = psuthreadinfo.uctx.uc_mcontext.gregs[ESP];
    //printf("ebp_lf %lu\n", ebp_last_frame);
    //printf("ebp_lf_lf %lu\n", *ebp_last_frame);
    uint32_t *esp_last_frame = ebp+2;
    uint32_t  ret_addr = *(ebp+1);
    psuthreadinfo.restart_point = ret_addr;
    psuthreadinfo.restart_point_offset = psuthreadinfo.restart_point - fn_start_addr;
    
    psuthreadinfo.num_elements = 0;
    psuthreadinfo.frame_index[0] = ebp_last_frame;//psuthreadinfo.num_blank_entries + ((unsigned long int)ebp_last_frame - (unsigned long int)esp_last_frame)/(STACK_WIDTH)+1;//fist ebp 
    for (int k = 1; k < NUM_FRAMES; k++){
        printf("ebp's last f %lu\n", (unsigned long int)ebp_last_frame);
        ebp_last_frame = *ebp_last_frame; //getting previous frame's ebp
        printf("ebp's new f%lu\n", (unsigned long int)ebp_last_frame);
        psuthreadinfo.frame_index[k] = ebp_last_frame;//psuthreadinfo.num_blank_entries + ((unsigned long int)ebp_last_frame - (unsigned long int)esp_last_frame)/(STACK_WIDTH)+1; //index of next frame
        }
    
        int k=0;
        printf("esp %lu\n", (unsigned long int)*esp_last_frame);
        for (i = 0; i < ((unsigned long int)ebp_last_frame-(unsigned long int)esp_last_frame)/(STACK_WIDTH)+1; i++)
        {
          psuthreadinfo.stack_raw_data[psuthreadinfo.num_blank_entries + i + psuthreadinfo.num_elements] = *(esp_last_frame+i);
          printf("writing to %d value: %lu", psuthreadinfo.num_blank_entries + i + psuthreadinfo.num_elements, *(esp_last_frame+i));
          if ((esp_last_frame+i) == psuthreadinfo.frame_index[k]){
            printf("<--- bp %d\n",k);
            psuthreadinfo.frame_index[k] = psuthreadinfo.num_blank_entries + i + psuthreadinfo.num_elements;
            k++;
          }
          else{
          printf("\n",k);
          }
        }
        psuthreadinfo.num_elements += i-1;
        //psuthreadinfo.frame_index[k] = psuthreadinfo.num_blank_entries + i;
        /*for(i=0;i<NUM_FRAMES;i++){
            printf("--%lu\n ", (unsigned long int)psuthreadinfo.frame_index[i]);
            printf("%lu\n", (unsigned long int)psuthreadinfo.stack_raw_data[psuthreadinfo.frame_index[i]]);
            
        }*/
    for (i = 0; i < 100; i++)
    {
        psuthreadinfo.stack_raw_data[psuthreadinfo.num_blank_entries+psuthreadinfo.num_elements+2+i] = *(ebp_last_frame+2+i);
    }
	
	printf("Need to send: EAX: %zx\n", psuthreadinfo.uctx.uc_mcontext.gregs[EAX] );
	
	// start a client right here. 
	// using this, send the context
	// store all info needed for that
	printf("migration starting\n");
	client(hostname); //sending uctx insinde of this function
	
	pthread_exit(&retval);
	
	return;
}