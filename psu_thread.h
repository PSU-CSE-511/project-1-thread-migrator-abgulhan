#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ucontext.h>
#include <pthread.h>

#define PORT 8080

//typedef struct psu_thread_info psu_thread_info_t;

void psu_thread_setup_init(int server_mode)
{
	if (server_mode) {
		// start a server
		// listen for connections
		// receive connection
		// receive the context from the client
		// close the server
		// switch to the new context
	}
	return;
}

int psu_thread_create(void * (*user_func)(void*), void *user_args)
{
	// start a pthread
	return 0; 
}

void psu_thread_migrate(const char *hostname)
{
	// save the context: c1
	// if I am a client
		// want to migrate to remote host now
		// start a client right here
		// try to connect. if failure, return failure msg
		// if success, then exit this thread
	// else, I am a server. simply continue
	return;
}
