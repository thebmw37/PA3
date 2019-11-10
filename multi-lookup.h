#ifndef MULTI_LOOKUP_H
#define MULTI_LOOKUP_H

#include <stdlib.h>		
#include <stdio.h>
#include <pthread.h>

//static int MAX_INPUT_FILES = 10;
static int MAX_RESOLVER_THREADS = 10;
static int MAX_REQUESTOR_THREADS = 5;
static int MAX_DOMAIN_NAME_LENGTH = 1025;
static int MAX_IP_LENGTH = 46;

typedef struct info {
	char* shared_buffer;
	pthread_mutex_t* m;
	pthread_cond_t* c_cons;
	pthread_cond_t* c_prod;
	FILE* file;
	FILE* requestor_log_file;
	pthread_t threadId;
	int* active_producers;
	int* num_files;
	char* file_names;
	int* file_name_ptr;
	int* active_producer_num;
	int* active_consumer_num;
	int producer_num;
	int* producer_priority;
	int* consumer_priority;
	int total_producers;
	int consumer_num;
} info;

void* parseFile(void* arg_struct);

void* convertFile(void* arg_struct);

int main(int argc, char* argv[]);

#endif
