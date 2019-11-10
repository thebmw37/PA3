/*Bryce Woods 
  brwo4424@colorado.edu
  10/28/2019*/

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "multi-lookup.h"
#include "util.h"

void* parseFile(void* arg_struct) {
	//Reading in information from the struct
	struct info* args = arg_struct;
	pthread_mutex_t* m = args->m;
	pthread_cond_t* c_cons = args->c_cons;
	pthread_cond_t* c_prod = args->c_prod;
	int* num_files = args->num_files;
	int this_producer = args->producer_num;
	int* num_producers = args->active_producer_num;
	int *producer_priority = args->producer_priority;
	FILE* log_file = args->requestor_log_file;

	int bigdone = 0; //Used for knowing when to terminate thread
	int file_count = 0; //Used for logging

	while(bigdone == 0) {
		char* read_file_name = malloc(256); //Name of file

		pthread_mutex_lock(m);
			char* buffer = args->shared_buffer;
			char c;

			int read_file_spot = 0;

			char* files = args->file_names;
			int* place_in_names = args->file_name_ptr;
			//Reading the name of the file
			for(int i = *place_in_names; (unsigned)i < strlen(files); i++) {
				if(files[i] == '\n') {
					*place_in_names = *place_in_names + 1;
					break;			
				}
				read_file_name[read_file_spot] = files[i]; //Building string with name
				read_file_spot++;
				*place_in_names = *place_in_names + 1;
			}
			*num_files = *num_files - 1;
		pthread_mutex_unlock(m);
		//If file name was empty, quit
		if(strlen(read_file_name) == 0) {
			break;
		} 
		//Attempt to open file
		FILE* readFile = fopen(read_file_name, "r");

		if(!readFile) { //If the file could not be opened, error
			fprintf(stderr, "Bad read file name!\n");
			break;
		}
		c = fgetc(readFile); //Get char from file
		char* readingBuff = malloc(MAX_DOMAIN_NAME_LENGTH); //For reading lines from file
		int readingBuffPos = 0;
		//Queue information
		int this_producer = args->producer_num;
		int* num_producers = args->active_producer_num;
		int *producer_priority = args->producer_priority;

		file_count++; //Incrementing number of files this producer has read

		while (c != EOF) 
		{
			while(producer_priority[0] != this_producer) {
				//Producer will wait, until its number is called, preventing starvation
			}

			pthread_mutex_lock(m);

				readingBuffPos = 0;
				while(c != '\n') { //Continuing until reaching end of line
					readingBuff[readingBuffPos] = c;
					readingBuffPos++;
					c = fgetc(readFile);
				}

				int space_in_buffer = 32 - strlen(buffer); //Amount of space in buffer

				//Block if there isn't space in the buffer for a string
				if(space_in_buffer < (int) strlen(readingBuff)) {
					pthread_cond_signal(c_cons);
					pthread_cond_wait(c_prod, m);
				}
				int pos = strlen(buffer);
				for(int i = 0; (unsigned)i < strlen(readingBuff); i++) {
					buffer[pos] = readingBuff[i]; //Writing to buffer
					pos++;
				}
				for(int i = strlen(readingBuff); i >= 0; i--) { //Cleaning readingBuff
					readingBuff[i] = '\000';
				}
				buffer[pos] = '\n'; //Writing an end of line to buffer
				pos++;
				c = fgetc(readFile); //Incrementing c and pos
				//Adjusting the queue so another producer can work
				for(int i = 0; i < *num_producers; i++) {
					if(i == *num_producers - 1) {
						producer_priority[i] = this_producer;
					}
					else {
						producer_priority[i] = producer_priority[i+1];
					}
				}
			pthread_mutex_unlock(m);
			pthread_cond_signal(c_cons);
		}
		pthread_cond_signal(c_cons);
		if(*num_files <= 0) { //If there are no more files to read, done
			bigdone = 1;
		}
		fclose(readFile); //Closing readFile
		free(readingBuff); //Freeing malloc'd memory
		free(read_file_name);
	}
	int done = 0;
	while(done == 0) { //Producer removes itself for the queue
		pthread_mutex_lock(m);
	
			int index = 0;
			for(int i = 0; i < *num_producers; i++) {
				if(producer_priority[i] == this_producer) {
					index = i; //Producer finds its index in the queue
				}
			}

			for(int i = index; i < *num_producers; i++) {
				if(i == *num_producers - 1) {
					producer_priority[i] = 0; //Sets itself to be 0
				}
				else { //Increments all other producers by 1
					producer_priority[i] = producer_priority[i+1];
				}
			}

			//printf("---THREAD %i TERMINATING---\n", this_producer);
			//Writing to the log file
			fputs("Thread ", log_file);
			fprintf(log_file, "%d", this_producer);
			fputs(" serviced ", log_file);
			fprintf(log_file, "%d", file_count);
			fputs(" files.\n", log_file);
			//Decrementing the number of active producers
			*num_producers = *num_producers - 1;
			done = 1;
		pthread_mutex_unlock(m);
	}
	pthread_cond_signal(c_cons);
	return NULL;
}

void* convertFile(void* arg_struct) {
	//Reading in information from struct
	struct info* args = arg_struct;
	pthread_mutex_t* m = args->m;
	pthread_cond_t* c_cons = args->c_cons;
	pthread_cond_t* c_prod = args->c_prod;
	int* num_prod = args->active_producer_num;
	int* consumer_priority = args->consumer_priority;
	int* num_consumers = args->active_consumer_num;
	int this_consumer = args->consumer_num;
	char* this_string = malloc(MAX_DOMAIN_NAME_LENGTH); //For reading from buffer

	//printf("\n---Initializing consumer %i---\n", this_consumer);
	int bigdone = 0;
	while(bigdone == 0) {
		while(consumer_priority[0] != this_consumer) {
			//Consumer waits for its number to come up
		}

		pthread_mutex_lock(m);
			char* buffer = args->shared_buffer;

			while(strlen(buffer) == 0 && *num_prod > 0) {
				//printf("\nBUFFER EMPTY, WAITING\n");
				pthread_cond_wait(c_cons, m);
			}

			if(strlen(buffer) <= 0 && *num_prod <= 0) {
				//printf("\nCONSUMER %i EXITING\n", this_consumer);
				bigdone = 1;
			}
			if(bigdone != 1) {

				//printf("\n\nCONSUMER %i WRITING:\n", this_consumer);

				FILE* outputFile = args->file;

				int i = 0;
				
				int str_i = 0;
				while((unsigned)i < strlen(buffer)) { //Reading string from buffer
					if(buffer[i] != '\n') {
						this_string[str_i] = buffer[i];
						str_i++;
					}
					else {
						char *ip = malloc(MAX_IP_LENGTH); //For storing ip
						if(strlen(this_string) > 0) { //Calling dnslookup
							if(dnslookup(this_string, ip, MAX_IP_LENGTH) != UTIL_FAILURE) {
								//Writing results
								fputs(this_string, outputFile);
								fputs(",", outputFile);
								fputs(ip, outputFile);
								fputs("\n", outputFile);
							}
							else { //If dnslookup failed
								fputs(this_string, outputFile);
								fputs(",", outputFile);
								fputs("\n", outputFile);
								fprintf(stderr, "Bad Hostname: %s\n", this_string);
							}
						}
						//Cleaning up this_string
						int q = strlen(this_string);
						while(q >= 0) {
							this_string[q] = '\000';
							q--;
						}
						//Cleaning up ip
						int m = strlen(ip);
						while(m >= 0) {
							ip[m] = '\000';
							m--;
						}
						free(ip);
						str_i = 0;
					}	
					i++;
				}
				i = strlen(buffer);
				while(i >= 0) { //Emptying the buffer
					buffer[i] = '\000';
					i--;
				}
				//Adjusting the consumers queue
				for(int i = 0; i < *num_consumers; i++) {
					if(i == *num_consumers - 1) {
						consumer_priority[i] = this_consumer;
					}
					else {
						consumer_priority[i] = consumer_priority[i+1];
					}
				}
			}
		pthread_mutex_unlock(m);
		pthread_cond_signal(c_prod);
	}
	int done = 0;
	while(done == 0) {
		pthread_mutex_lock(m);
	
			int index = 0;
			for(int i = 0; i < *num_consumers; i++) {
				if(consumer_priority[i] == this_consumer) {
					index = i; //Consumer finds its index
				}
			}

			for(int i = index; i < *num_consumers; i++) {
				if(i == *num_consumers - 1) { //Sets itself to 0
					consumer_priority[i] = 0;
				}
				else { //Increments all other consumers by 1
					consumer_priority[i] = consumer_priority[i+1];
				}
			}

			//printf("---CONSUMER %i EXITING---\n", this_consumer);
			*num_consumers = *num_consumers - 1;
			done = 1;
		pthread_mutex_unlock(m);
	}
	pthread_cond_signal(c_prod);

	free(this_string);

	return NULL;
}

int main(int argc, char* argv[]) {
	clock_t begin = clock(); //Starting clock to time program

	if(argc < 5) {	//Testing arguments
		fprintf(stderr, "Not enough arguments\n");
		return(-1);
	}
	//Getting command line arguments
	int num_requestor_threads = atoi(argv[1]);
	int num_resolver_threads = atoi(argv[2]);
	char* parsing_log_name = argv[3];
	char* conversion_log_name = argv[4];
	pthread_t thread_Ids[MAX_RESOLVER_THREADS];
	pthread_t consumer_thread_Ids[MAX_REQUESTOR_THREADS];
	//Handling input errors
	if(num_requestor_threads > MAX_REQUESTOR_THREADS) {
		fprintf(stderr, "Too many requestor threads!\n");
		exit(EXIT_FAILURE);
	}
	if(num_resolver_threads > MAX_RESOLVER_THREADS) {
		fprintf(stderr, "Too many resolver threads!\n");
		exit(EXIT_FAILURE);
	}
	//Opening files from command line arguments
	FILE* outputFile = fopen(conversion_log_name, "w");
	FILE* requestor_log_file = fopen(parsing_log_name, "w");
	//Testing if the output file could be opened successfully
	if(!outputFile) {
		fprintf(stderr, "Bad output filename!\n");
		exit(EXIT_FAILURE);
	}
	//Allocating the shared buffer
	char* buffer = malloc(MAX_DOMAIN_NAME_LENGTH);
	//Initializing mutexes and condition variables
	pthread_mutex_t m1 = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t c_cons = PTHREAD_COND_INITIALIZER;
	pthread_cond_t c_prod = PTHREAD_COND_INITIALIZER;
	//Creating a pointer to the number of input files
	int num_files = argc - 5;
	int* ptr_num_files = &num_files;
	//Creating buffer to store files
	char* files = malloc(1024);
	//Creating pointer to the current name being accessed for file names
	int place_in_names = 0;
	int* file_name_ptr = &place_in_names;
	//Loading the files buffer with the names of files
	for(int i = 0; i < num_files; i++) {
		char* file_name = malloc(strlen("input/") + strlen(argv[5 + i]) + 1);
    		strcpy(file_name, "input/");
    		strcat(file_name, argv[5 + i]);
		strcat(file_name, "\n");
		int buff_len = strlen(files);
		for(int j = 0; (unsigned)j < strlen(file_name); j++) {
			files[buff_len + j] = file_name[j];
		}
		free(file_name);
	}
	//Creating pointers for the number of active producers and consumers
	int active_producers = 0;
	int active_consumers = 0;
	int* producers = &active_producers;
	int* consumers = &active_consumers;
	//Creating array for the producer queue, used to prevent starvation (init: 1,2,3,etc)
	int* producer_queue = malloc(num_requestor_threads);
	for(int i = 0; i < num_requestor_threads; i++) {
		producer_queue[i] = i + 1;
	}
	//Creating array for the consumer queue, used to prevent starvation (init: 1,2,3,etc)
	int* consumer_queue = malloc(num_resolver_threads);
	for(int i = 0; i < num_resolver_threads; i++) {
		consumer_queue[i] = i + 1;
	}
	//Creating arg struct for producers
	struct info arg_structs[num_requestor_threads];

	for(int i = 0; i < num_requestor_threads; i++) {
		arg_structs[i].shared_buffer = buffer; //Shared buffer
		arg_structs[i].m = &m1; //Mutex
		arg_structs[i].c_cons = &c_cons; //Consumer condition var
		arg_structs[i].c_prod = &c_prod; //Producer condition var
		arg_structs[i].num_files = ptr_num_files; //Number of remaining files
		arg_structs[i].file_names = files; //File names
		arg_structs[i].file_name_ptr = file_name_ptr; //Location in file names
		arg_structs[i].active_producer_num = producers; //Number of active producers
		arg_structs[i].producer_num = i + 1; //This producer's number
		arg_structs[i].producer_priority = producer_queue; //The producer's queue
		arg_structs[i].total_producers = num_requestor_threads; //Total number of producers
		arg_structs[i].requestor_log_file = requestor_log_file; //Log file passed in from cmdline
		pthread_t parsingThreadId; //Thread Id of this producer
		pthread_create(&parsingThreadId, NULL, parseFile, &(arg_structs[i])); //Creating the thread
		thread_Ids[i] = parsingThreadId; //Adding this thread to the array of threads
		active_producers++; //Incrementing the number of active producers
	}
	//Struct for consumer threads
	struct info out_struct[num_resolver_threads];

	for(int i = 0; i < num_resolver_threads; i++) {
		out_struct[i].shared_buffer = buffer; //Shared buffer
		out_struct[i].m = &m1; //Mutex
		out_struct[i].c_cons = &c_cons; //Consumer condition var
		out_struct[i].c_prod = &c_prod; //Producer condiiton var
		out_struct[i].file = outputFile; //output file
		out_struct[i].active_producer_num = producers; //Number of active producers
		out_struct[i].consumer_num = i + 1; //This consumer's number
		out_struct[i].active_consumer_num = consumers; //Active consumer
		out_struct[i].consumer_priority = consumer_queue; //Queue for consumers
		pthread_t conversionThreadId; //Thread Id of this consumer
		pthread_create(&conversionThreadId, NULL, convertFile, &(out_struct[i])); //Creating the thread
		consumer_thread_Ids[i] = conversionThreadId; //Adding this thread to the array of threads
		active_consumers++; //Incrementing the number of active consumers
	}

	for(int i = 0; i < num_requestor_threads; i++) { //Waiting for producers to finish
		pthread_join(thread_Ids[i], NULL);
		
	}

	for(int i = 0; i < num_resolver_threads; i++) { //Waiting for consumers to finish
		pthread_join(consumer_thread_Ids[i], NULL);
	}
	//Closing open files
	fclose(outputFile);
	fclose(requestor_log_file);
	//Freeing up what was allocated with malloc()
	free(buffer);
	free(files);
	free(producer_queue);
	free(consumer_queue);
	//Stopping the clock and displaying time
	clock_t end = clock();
	double time_spent = ((double)(end - begin) / CLOCKS_PER_SEC) / 2;
	printf("Time elapsed: %lf seconds.\n", time_spent);
}