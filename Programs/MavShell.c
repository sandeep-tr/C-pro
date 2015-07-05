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
 * Description: A custom shell which can execute selected commands
 * issued by the User. Program uses fork() and exec() to run the commands
 * User issued.
 */
 
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include <stdarg.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>

/*
 * Constants
 */
const int BUFFER_SIZE = 100;
const int ARGS_SUPPORTED = 4;
const int CUSCH_EXIT = 99;
const char *PROMPT = "msh>";
const char *TOKENT_SPLR = " ";
const char *NOT_FOUND = "%s: Command not found.\n";
const char *SHELL_ERROR = "-msh: ";
const char *EXIT_COMMAND[] = { "exit", "quit" };
const char *COMMAND_DIRS[] = { "/bin/", "/usr/bin/" };

/*
 * Function prototypes
 */
void execute_command(char *[]);
int fork_process(char *[]);
void change_dir(char *[]);
void show_prompt();
void interrupt_handler(int);
void split_string(char *, char *[]);
char *concat(const char *, ...);
int is_empty(const char *);

/*
 * Function: main
 * Returns: exit status of the program
 * Description: The main controller of the whole program,
 * connects with other functions and accomplishes the given task.
 */
int main() {
	sig_t signal_return = signal(SIGINT, interrupt_handler);
	assert(signal_return != SIG_ERR);
	signal_return = signal(SIGTSTP, interrupt_handler);
	assert(signal_return != SIG_ERR);

	show_prompt();
	char buffer[BUFFER_SIZE], *shell_args[ARGS_SUPPORTED + 1];
	while (fgets(buffer, BUFFER_SIZE, stdin)) {
		// If read input contains only newline/ is empty, do nothing and show prompt
		// else if the last character is newline, replace it with null terminator. Doing so
		// will help while splitting the read line.
		if (buffer[0] == '\n' || is_empty(buffer)) {
			show_prompt();
			continue;
		} else if (buffer[strlen(buffer) - 1] == '\n') {
			buffer[strlen(buffer) - 1] = 0;
		}
		split_string(buffer, shell_args);
		if (!strcmp(shell_args[0], EXIT_COMMAND[0])
				|| !strcmp(shell_args[0], EXIT_COMMAND[1])) {
			exit(EXIT_SUCCESS);
		}
		execute_command(shell_args);
		show_prompt();
	}
	return EXIT_SUCCESS;
}

/*
 * Function: execute_command
 * Parameter(s): shell_args - array of shell args required for performing exec
 * Description: A decision making function. Calls fork_process function if the shell command
 * passed is to be executed using fork/exec, else it calls inbuilt function
 * the last received satellite update.
 */
void execute_command(char *shell_args[]) {
	// If command is "cd" execute the command directly using chdir() provided by 'C', from the parent.
	// No fork() and exec() is not used for "cd", since executing chdir() from child process only changes
	// the child processes directory and not of the parent's.
	if (!strcmp(shell_args[0], "cd")) {
		change_dir(shell_args);
	} else {
		int child_status = fork_process(shell_args);
		if (CUSCH_EXIT == child_status) {
			printf(NOT_FOUND, shell_args[0]);
			fflush(NULL);
		}
	}
}

/*
 * Function: fork_process
 * Parameter(s): shell_args - array of shell args required for performing exec
 * Returns: An integer value of 0 for success, 99(custom) for any errors resulting
 * from exec
 * Description: Creates a child process using fork(), which calls exec to run an utility
 */
int fork_process(char *shell_args[]) {
	pid_t child_pid = fork();
	int child_status = -1;

	if (child_pid == -1) {
		perror("Fork failed");
	} else if (child_pid == 0) {
		// If the first execv executes successfully, the utility called by execv exits itself
		// and status will be received by parent. Else, the program control is returned and continues to
		// execute the code below it.
		char *file_path = concat(COMMAND_DIRS[0], shell_args[0], NULL);
		execv(file_path, shell_args);
		free(file_path);
		file_path = concat(COMMAND_DIRS[1], shell_args[0], NULL);
		execv(file_path, shell_args);
		free(file_path);

		_exit(CUSCH_EXIT);
	} else {
		(void) waitpid(child_pid, &child_status, 0);
		if (WIFEXITED(child_status)) {
			child_status = WEXITSTATUS(child_status);
		}
	}
	return child_status;
}

/*
 * Function: change_dir
 * Parameter(s): args - array of shell args required for inbuilt c functions
 * Description: Executes built in shell functions, instead of calling an utility via exec
 */
void change_dir(char *args[]) {
	if (args[1]) {
		int status = chdir(args[1]);
		if (status) {
			char *error_message = concat(SHELL_ERROR, args[0], ": ", args[1],
			NULL);
			perror(error_message);
			free(error_message);
		}
	}
}

/*
 * Function: show_prompt
 * Description: Prints the prompt to the Interface.
 */
void show_prompt() {
	printf("%s", PROMPT);
	fflush(NULL);
}

/*
 * Function: interrupt_handler
 * Parameter(s): signal_num - interrupt signal on which handler needs to be
 * registered.
 * Description: Executes the commands in this function for the commands registered for this handler.
 */
void interrupt_handler(int signal_num) {
	printf("\n%s", PROMPT);
	fflush(NULL);
}

/*
 * Function: split_string
 * Parameter(s): string - command line submitted by User
 * args - storage passed from calling function to store split items
 * Description: Splits the command line passed to function, based on space
 * between words. This helps in extracting the command and its arguments
 */
void split_string(char *string, char *args[]) {
	char *token;
	int counter = 0;

	token = strtok(string, TOKENT_SPLR);
	while (token != NULL || counter < ARGS_SUPPORTED) {
		args[counter] = token;
		token = strtok(NULL, TOKENT_SPLR);
		++counter;
	}
	// Array to store tokens have a predefined size. If there aren't
	// enough tokens to fill up the array, left space is filled up with NULL
	while (counter < ARGS_SUPPORTED + 1) {
		args[counter] = NULL;
		++counter;
	}
}

/*
 * Function: concat
 * Parameter(s): string ... - strings to be concatenated
 * Returns: Concatenated string
 * Description: Concatenates all the passed string params into this variadic function.
 * Reference: Delan Azabani, Oliver Charlesworth. About variadic functions available at
 * http://stackoverflow.com/questions/4117355/memory-allocation-for-simple-variadic-string-concatenation-in-c
 */
char *concat(const char *string, ...) {
	va_list vl;
	va_start(vl, string);
	char *result = (char *) malloc(strlen(string) + 1);
	char *next;
	strcpy(result, string);
	while ((next = va_arg(vl, char *))) {
		result = (char *) realloc(result, strlen(result) + strlen(next) + 1);
		strcat(result, next);
	}
	return result;
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
