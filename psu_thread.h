#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ucontext.h>
#include <pthread.h>

//you write the code here

//typedef struct psu_thread_info psu_thread_info_t;

void psu_thread_setup_init(int mode)
{
	//Read from a file to set up the socket connection between the client and the server

	return;
}

int psu_thread_create(void * (*user_func)(void*), void *user_args)
{
	// make thread related setup
	// create thread and start running the function based on *user_func
	
	return 0; 
}

void psu_thread_migrate(const char *hostname)
{
	//thread Migration related code
	return;
}