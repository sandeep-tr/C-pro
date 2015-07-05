/*The MIT License (MIT)

 Copyright (c) 2014 Sandeep Raveendran Thandassery

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 */
 
 /*
 * Description: A scheduler model which can demonstrate how
 * scheduling is done in Operating Systems based on following algorithms
 *  - First Come First Served
 *  - Shortest Job Next
 *  - Shortest Job Next with Preemption
 *  - Priority
 *  - Priority with Preemption
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

/*
 * Constants
 */
const int ARG_LIMIT = 6;
const int BUFFER_SIZE = 512;
const char *TOKENT_SPLR = ", ";

/*
 * Custom Types
 */
typedef struct Job {
	pid_t id;
	float arrival_time;
	float run_time;
	int priority;
} Job;

typedef struct Process {
	Job *job;
	struct Process *prev;
	struct Process *next;
} Process;

typedef enum {
	FCFS, SJN, SJNPRE, PRI, PRIPRE
} Scheduler;

/*
 * Global variables
 */
float time_quantum = 1;
char *job_file = NULL;
Scheduler scheduler = FCFS;
Process *head_link = NULL;

/*
 * Function prototypes
 */
void read_args(int, char *[]);
void read_jobs();

void start_scheduler();
void FCFS_scheduler();
void SJN_scheduler();
void SJNPRE_scheduler();
void PRI_scheduler();
void PRIPRE_scheduler();

void sort_by_arrival();
void remove_process(Process*);
void print_util(pid_t, int, int);
void handle_error(char*);
int is_empty(const char*);

/*
 * Function: main
 * Parameter(s): built in parameters that has command line arguments stored in it.
 * Returns: exit status of the program
 * Description: The main controller of the whole program,
 * connects with other functions and accomplishes the given task.
 */
int main(int argc, char* argv[]) {

	read_args(argc, argv);
	read_jobs();
	start_scheduler();

	return EXIT_SUCCESS;
}

/*
 * Function: read_args
 * Parameter(s): argc - no of command line arguments passed
 * argv - string array containing command line arguments
 * Description: Reads, processes and validates the command line arguments passed
 * to the program.
 */
void read_args(int argc, char *argv[]) {
	if (argc > ARG_LIMIT) {
		handle_error("Too many arguments. Exiting program.\n");
	}
	int algorithm = 0, file = 0, counter, skip_next = 0;
	for (counter = 1; counter < argc; counter++) {
		// Flags '-a' and '-q' are read together with its succeeding argument.
		// So a skip is done to avoid re-reading the succeeding argument again.
		if (skip_next) {
			--skip_next;
			continue;
		}
		if (!strcmp("-a", argv[counter])) {
			if (!strcmp("FCFS", argv[counter + 1])) {
				scheduler = FCFS;
			} else if (!strcmp("SJN", argv[counter + 1])) {
				scheduler = SJN;
			} else if (!strcmp("SJNPRE", argv[counter + 1])) {
				scheduler = SJNPRE;
			} else if (!strcmp("PRI", argv[counter + 1])) {
				scheduler = PRI;
			} else if (!strcmp("PRIPRE", argv[counter + 1])) {
				scheduler = PRIPRE;
			} else {
				handle_error("Invalid scheduling algorithm\n");
			}
			algorithm = 1;
			++skip_next;
		} else if (!strcmp("-q", argv[counter])) {
			time_quantum = atof(argv[counter + 1]);
			++skip_next;
		} else {
			job_file = (argv[counter]);
			file = 1;
		}
	}
	if (!algorithm) {
		handle_error("Scheduling algorithm not found. Exiting program.\n");
	} else if (!file) {
		handle_error("Job file not found. Exiting program.\n");
	}
}

/*
 * Function: read_jobs
 * Description: Reads the input job file, read the jobs given in
 * predefined format and store them for later processing.
 */
void read_jobs() {
	char buffer[BUFFER_SIZE];
	Process *process = NULL;
	FILE *file = fopen(job_file, "rt");
	if (file == NULL) {
		handle_error("Job file not found\n");
	}
	while (fgets(buffer, BUFFER_SIZE, file)) {
		if (buffer[0] == '\n' || buffer[0] == '#' || is_empty(buffer)) {
			continue;
		}
		process = (Process*) calloc(1, sizeof(Process));
		process->job = (Job*) calloc(1, sizeof(Job));
		process->job->id = strtol(strtok(buffer, TOKENT_SPLR), NULL, 10);
		process->job->arrival_time = (float) strtol(strtok(NULL, TOKENT_SPLR),
		NULL, 10);
		process->job->run_time = (float) strtol(strtok(NULL, TOKENT_SPLR),
		NULL, 10);
		process->job->priority = strtol(strtok(NULL, TOKENT_SPLR),
		NULL, 10);
		if (head_link == NULL) {
			process->prev = NULL;
		} else {
			head_link->prev = process;
		}
		process->next = head_link;
		head_link = process;
	}
	fclose(file);
}

/*
 * Function: start_scheduler
 * Description: Calls the scheduler algorithm as per given by the User.
 * Acts as a selector.
 */
void start_scheduler() {
	sort_by_arrival();

	switch (scheduler) {
	case FCFS:
		FCFS_scheduler();
		break;
	case SJN:
		SJN_scheduler();
		break;
	case SJNPRE:
		SJNPRE_scheduler();
		break;
	case PRI:
		PRI_scheduler();
		break;
	case PRIPRE:
		PRIPRE_scheduler();
		break;
	default:
		FCFS_scheduler();
		break;
	}
}

/*
 * Function: FCFS_scheduler
 * Description: Process jobs as per First Come First Served algorithm.
 */
void FCFS_scheduler() {
	float timer = 0, quantum_used = 0;
	Process *current = head_link, *selected = NULL;
	while (1) {
		current = head_link;
		if (current == NULL) {
			break;
		}
		// If selected is not NULL, it means the prev. selected process hasn't finished its processing
		if (selected == NULL) {
			while (1) {
				// Iteratively selects the job which came first to the scheduler queue
				if (current != NULL && current->job->arrival_time <= timer) {
					if (selected == NULL
							|| current->job->arrival_time
									< selected->job->arrival_time
							|| (current->job->arrival_time
									== selected->job->arrival_time
									&& current->job->id < selected->job->id)) {
						selected = current;
					}
					current = current->next;
				} else if (current != NULL && selected == NULL) {
					timer += time_quantum;
				} else {
					break;
				}
			}
		}

		timer += time_quantum, quantum_used += time_quantum;
		selected->job->run_time -= time_quantum;
		if (selected->job->run_time <= 0) {
			print_util(selected->job->id, timer - quantum_used, timer);
			remove_process(selected);
			selected = NULL, quantum_used = 0;
		}
	}
}

/*
 * Function: SJN_scheduler
 * Description: Processes jobs as per Shortest Job Next algorithm.
 * Shortest jobs are the ones with shortest run time at a point of time.
 */
void SJN_scheduler() {
	float timer = 0, quantum_used = 0;
	Process *current = head_link, *selected = NULL;
	while (1) {
		current = head_link;
		if (current == NULL) {
			break;
		}
		// If selected is not NULL, it means the prev. selected job hasn't finished its processing
		if (selected == NULL) {
			while (1) {
				// Iteratively selects the job which has shortest run time and incase of a tie, one
				// with lowest id is selected for processing
				if (current != NULL && current->job->arrival_time <= timer) {
					if (selected == NULL
							|| current->job->run_time < selected->job->run_time
							|| (current->job->run_time
									== selected->job->run_time
									&& current->job->id < selected->job->id)) {
						selected = current;
					}
					current = current->next;
				} else if (current != NULL && selected == NULL) {
					timer += time_quantum;
				} else {
					break;
				}
			}
		}

		timer += time_quantum, quantum_used += time_quantum;
		selected->job->run_time -= time_quantum;
		if (selected->job->run_time <= 0) {
			print_util(selected->job->id, timer - quantum_used, timer);
			remove_process(selected);
			selected = NULL, quantum_used = 0;
		}
	}
}

/*
 * Function: SJNPRE_scheduler
 * Description: Process jobs as per Shortest Job Next with Preemption algorithm.
 * Check for preemption is done at given time quantum.
 */
void SJNPRE_scheduler() {
	float timer = 0, quantum_used = 0;
	Process *current = head_link, *running = NULL, *selected = NULL;
	while (1) {
		current = head_link, running = selected;
		if (current == NULL) {
			break;
		}
		while (1) {
			// Iteratively selects the job which has shortest run time and incase of a tie, one
			// with lowest id is selected for processing
			if (current != NULL && current->job->arrival_time <= timer) {
				if (selected == NULL
						|| current->job->run_time < selected->job->run_time
						|| (current->job->run_time == selected->job->run_time
								&& current->job->id < selected->job->id)) {
					selected = current;
				}
				current = current->next;
			} else if (current != NULL && selected == NULL) {
				timer += time_quantum;
			} else {
				break;
			}
		}
		// running stores the prev. 'process/process state'. If it is different from selected,
		// then 'process/process state' has changed and details are logged
		if (running == NULL || running->job->id == selected->job->id) {
			timer += time_quantum, quantum_used += time_quantum;
			selected->job->run_time -= time_quantum;
			if (selected->job->run_time <= 0) {
				print_util(selected->job->id, timer - quantum_used, timer);
				remove_process(selected);
				selected = NULL, quantum_used = 0;
			}
		} else {
			print_util(running->job->id, timer - quantum_used, timer);
			quantum_used = 0;
		}
	}
}

/*
 * Function: PRI_scheduler
 * Description: Process jobs on the basis of priorities assigned to them.
 * Highest priority jobs get processed first. Higher priority jobs are the ones
 * having lesser priority values.
 */
void PRI_scheduler() {
	float timer = 0, quantum_used = 0;
	Process *current = head_link, *selected = NULL;
	while (1) {
		current = head_link;
		if (current == NULL) {
			break;
		}
		// If selected is not NULL, it means the prev. selected job hasn't finished with its processing
		if (selected == NULL) {
			while (1) {
				// Iteratively selects the job which has highest priority at given point of time
				// and incase of a tie, selects the one with lowest id for processing
				if (current != NULL && current->job->arrival_time <= timer) {
					if (selected == NULL
							|| current->job->priority < selected->job->priority
							|| (current->job->priority
									== selected->job->priority
									&& current->job->id < selected->job->id)) {
						selected = current;
					}
					current = current->next;
				} else if (current != NULL && selected == NULL) {
					timer += time_quantum;
				} else {
					break;
				}
			}
		}

		timer += time_quantum, quantum_used += time_quantum;
		selected->job->run_time -= time_quantum;
		if (selected->job->run_time <= 0) {
			print_util(selected->job->id, timer - quantum_used, timer);
			remove_process(selected);
			selected = NULL, quantum_used = 0;
		}
	}
}

/*
 * Function: PRIPRE_scheduler
 * Description: Processes jobs based on Priority with Preemption algorithm.
 * Checks for preemption at given time quantum.
 */
void PRIPRE_scheduler() {
	float timer = 0, quantum_used = 0;
	Process *current = head_link, *running = NULL, *selected = NULL;
	while (1) {
		current = head_link, running = selected;
		if (current == NULL) {
			break;
		}
		while (1) {
			// Iteratively selects the job which has highest priority at given point of time
			// and incase of a tie, selects the one with lowest id for processing
			if (current != NULL && current->job->arrival_time <= timer) {
				if (selected == NULL
						|| current->job->priority < selected->job->priority
						|| (current->job->priority == selected->job->priority
								&& current->job->id < selected->job->id)) {
					selected = current;
				}
				current = current->next;
			} else if (current != NULL && selected == NULL) {
				timer += time_quantum;
			} else {
				break;
			}
		}

		// running stores the prev. 'process/process state'. If it is different from selected,
		// then 'process/process state' has changed and details are logged
		if (running == NULL || running->job->id == selected->job->id) {
			timer += time_quantum, quantum_used += time_quantum;
			selected->job->run_time -= time_quantum;
			if (selected->job->run_time <= 0) {
				print_util(selected->job->id, timer - quantum_used, timer);
				remove_process(selected);
				selected = NULL, quantum_used = 0;
			}
		} else {
			print_util(running->job->id, timer - quantum_used, timer);
			quantum_used = 0;
		}
	}
}

/*
 * Function: sort_by_arrival
 * Description: Utility function - sorts the processes read from file
 * based on its arrival time. Based on the assumption - input may not be in
 * sorted order
 */
void sort_by_arrival() {
	Process *current = head_link, *comparator, *temp;
	Job *current_job;
	while (current != NULL) {
		temp = current, comparator = current->next;
		while (comparator != NULL) {
			if (comparator->job->arrival_time < temp->job->arrival_time
					|| (comparator->job->arrival_time == temp->job->arrival_time
							&& comparator->job->id < temp->job->id)) {
				temp = comparator;
			}
			comparator = comparator->next;
		}

		if (temp != NULL) {
			current_job = temp->job;
			temp->job = current->job;
			current->job = current_job;
		}
		current = current->next;
	}
}

/*
 * Function: remove_process
 * Parameter(s): process - the process who's allocated memory needs to be freed
 * Description: Frees allocated space for the passed process from heap.
 */
void remove_process(Process *process) {
	Process *temp = NULL;
	if (process->prev == NULL && process->next == NULL) {
		head_link = NULL;
	} else if (process->prev == NULL) {
		temp = process->next;
		temp->prev = NULL;
		head_link = temp;
	} else if (process->next == NULL) {
		temp = process->prev;
		temp->next = NULL;
	} else {
		temp = process->prev;
		temp->next = process->next;
		temp = process->next;
		temp->prev = process->prev;
	}
	free(process->job);
	free(process);
}

/*
 * Function: print_util
 * Parameter(s): id - process id
 * start_time - process start time
 * end_time - process end time
 * Description: Prints the process execution details.
 */
void print_util(pid_t id, int start_time, int end_time) {
	printf("%d, %d, %d\n", id, start_time, end_time);
}

/*
 * Function: handle_error
 * Parameter(s): message - to be printed
 * Description: Utility function - prints the passed message to the console
 * and exits the program.
 */
void handle_error(char *message) {
	printf("error: %s", message);
	exit(EXIT_SUCCESS);
}
/*
 * Function: is_empty
 * Parameter(s): string - string to be validated
 * Returns: Validation status
 * Description: Validates if the passed string is empty or not.
 */
int is_empty(const char *string) {
	while (*string != '\0') {
		if (!isspace(*string)) {
			return 0;
		}
		string++;
	}
	return 1;
}
