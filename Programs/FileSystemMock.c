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
 * Description: A custom file system model to
 * showcase the general features of a modern file system. 
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <regex.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#define FILE_SYSTEM_SIZE 1310720 //-- 10MB
#define BLOCK_SIZE 2048 //-- Bytes
#define DIRECTORY_SIZE 128
#define TOTAL_BLOCKS (FILE_SYSTEM_SIZE/BLOCK_SIZE)
#define MAX_FILE_SIZE 98304
#define MAX_FILE_BLOCKS (MAX_FILE_SIZE/BLOCK_SIZE)

typedef enum State {
	NEGATIVE = -1, UNSET = 0, SET = 1
} State;

/*
 * Constants
 */
const int BUFFER_SIZE = 600;
const int MAX_FILE_NAME = 255;
const int ARGS_SUPPORTED = 3;
const int CUSCH_EXIT = 99;
const char *PROMPT = "mfs>";
const char *TOKENT_SPLR = " ";
const char *NOT_FOUND = "%s: Command not found.\n";
const char *FILE_NAME_REGEX = "^[a-zA-Z0-9.]{1,255}$";

/*Custom types*/
typedef struct Inode {
	short used;
	char file_name[255];
	unsigned long size;
	time_t time_created;
	int blocks[MAX_FILE_BLOCKS];
} Inode;

typedef struct Directory {
	struct Inode files[DIRECTORY_SIZE];
} Directory;

Directory directory;
unsigned char file_data[TOTAL_BLOCKS][2048];
unsigned int used_space[TOTAL_BLOCKS];

/*Function prototypes*/
void execute_command(char *[]);
void put(char *[]);
void get(char *[]);
void delete(char *);
void list();
long disk_availability(short);
void show_prompt(short);
void print_message(char *);
void split_string(char *, char *[]);
int is_empty(const char *);
int get_new_file_entry();
int get_free_block();

/*
 * Function: main
 * Returns: exit status of the program
 * Description: The main controller of the whole program,
 * connects with other functions and accomplishes the given task.
 */
int main(void) {
	show_prompt(0);
	char buffer[BUFFER_SIZE], *shell_args[ARGS_SUPPORTED];
	while (fgets(buffer, BUFFER_SIZE, stdin)) {
		// If read input contains only newline/ is empty, do nothing and show prompt
		// else if the last character is newline, replace it with null terminator. Doing so
		// will help while splitting the read line.
		if (buffer[0] == '\n' || is_empty(buffer)) {
			show_prompt(0);
			continue;
		} else if (buffer[strlen(buffer) - 1] == '\n') {
			buffer[strlen(buffer) - 1] = 0;
		}
		split_string(buffer, shell_args);

		execute_command(shell_args);
		show_prompt(0);
	}
	return EXIT_SUCCESS;
}

/*
 * Function: execute_command
 * Parameter(s): shell_args - array of arguments read from command line
 * Description: A decision making function. Calls appropriate function based on
 * the command passed by User.
 */
void execute_command(char *shell_args[]) {

	if (!strcmp(shell_args[0], "put")) {
		put(shell_args);
	} else if (!strcmp(shell_args[0], "get")) {
		get(shell_args);
	} else if (!strcmp(shell_args[0], "del")) {
		delete(shell_args[1]);
	} else if (!strcmp(shell_args[0], "list")) {
		list();
	} else if (!strcmp(shell_args[0], "df")) {
		disk_availability(1);
	} else {
		printf("%s%s: Command not found\n", PROMPT, shell_args[0]);
		fflush(NULL);
	}
}

/*
 * Function: put
 * Parameter(s): args - array containing command parameters
 * Description: Copies the given file from OS file system to the MAV file system.
 */
void put(char *args[]) {

	//-- Checks if the file name argument is missing or not.
	if (args[1] == NULL) {
		print_message("put error: File name missing.");
		return;
	}
	//-- Checks if the file name length exceeds the predefined limit.
	if (strlen(args[1]) > MAX_FILE_NAME) {
		print_message("put error: File name too long.");
		return;
	}

	//-- Checks if the file name contains any invalid characters (i.e other than
	//-- alphanumeric characters and periods.) using regex.
	regex_t regex;
	if (regcomp(&regex, FILE_NAME_REGEX, REG_EXTENDED) != 0) {
		print_message("Failed to compile regex");
		return;
	}
	if (regexec(&regex, args[1], (size_t) 0, NULL, 0)) {
		print_message("put error: Invalid file name.");
		regfree(&regex);
		return;
	}
	regfree(&regex);

	//-- Checks if the file is available in the OS file system.
	struct stat buf;
	int status = stat(args[1], &buf);
	if (status == -1) {
		print_message("put error: File not found.");
		return;
	}
	//-- Checks if the file exceeds the size supported by MAV file system.
	unsigned long file_size = (int) buf.st_size;
	if (file_size > MAX_FILE_SIZE) {
		print_message("put error: Exceeds maximum supported file size.");
		return;
	}
	//-- Checks if MAV disk has enough space to store the file.
	unsigned long available_disk_space = disk_availability(0);
	if (file_size > available_disk_space) {
		print_message("put error: Not enough disk space.");
		return;
	}

	int file_entry_index = get_new_file_entry();
	if (file_entry_index == -1) {
		print_message("put error: Directory limit reached.");
		return;
	}

	int counter = 0, offset = 0, new_block = 0;
	long copy_size = file_size;
	// -- Open the input file read-only
	// -- Copy file section. Reference: File read sample code provided by Prof. Trevor Bakker, UTArlington.
	FILE *input_file = fopen(args[1], "r");
	while (copy_size > 0) {

		new_block = get_free_block();
		//-- After each write, seek to the position which is yet to be written.
		fseek(input_file, offset, SEEK_SET);
		int bytes = fread(file_data[new_block], BLOCK_SIZE, 1, input_file);
		if (bytes == 0 && !feof(input_file)) {
			print_message(
					"get error: An error occurred reading from the input file.");
			return;
		}
		// -- Clear the EOF file flag.
		clearerr(input_file);
		copy_size -= BLOCK_SIZE;
		offset += BLOCK_SIZE;
		directory.files[file_entry_index].blocks[counter++] = new_block;
		used_space[new_block] = SET;
	}
	//-- Close the file stream once usage is over.
	fclose(input_file);
	while (counter < MAX_FILE_BLOCKS) {
		directory.files[file_entry_index].blocks[counter++] = NEGATIVE;
	}
	memcpy(directory.files[file_entry_index].file_name, args[1],
			strlen(args[1]) + 1);
	directory.files[file_entry_index].size = file_size;
	directory.files[file_entry_index].time_created = time(NULL);
	directory.files[file_entry_index].used = SET;
}

/*
 * Function: get
 * Parameter(s): args - array containing command parameters
 * Description: Copies the given file from MAV file system to the OS file system.
 * If a args array has a third parameter, the copied file will be assigned that parameter
 * as its name.
 */
void get(char *args[]) {

	if (args[1] == NULL) {
		print_message("get error: File name not found");
		return;
	}
	int counter = 0, index = -1;
	while (counter < DIRECTORY_SIZE) {
		if (!strcmp(directory.files[counter].file_name, args[1])) {
			index = counter;
		}
		++counter;
	}
	if (index < 0) {
		print_message("get error: File not found");
		return;
	}

	counter = 0;
	int block_index = 0, num_bytes = 0;
	long offset = 0, copy_size = directory.files[index].size;
	char *copy_name = (args[2] == NULL) ? args[1] : args[2];
	// -- Copy file section. Reference: File write sample code provided by Prof. Trevor Bakker, UTArlington.
	FILE *output_file = fopen(copy_name, "w");
	while (copy_size > 0) {

		// If the remaining number of bytes we need to copy is less than BLOCK_SIZE then
		// only copy the amount that remains. If we copied BLOCK_SIZE number of bytes we'd
		// end up with garbage at the end of the file.
		if (copy_size < BLOCK_SIZE) {
			num_bytes = copy_size;
		} else {
			num_bytes = BLOCK_SIZE;
		}
		block_index = directory.files[index].blocks[counter];
		// -- Move the pointer in the output stream to the number of bytes already wrote.
		fseek(output_file, offset, SEEK_SET);
		//-- Write num_bytes number of bytes from our data array into our output file.
		fwrite(file_data[block_index], num_bytes, 1, output_file);

		copy_size -= BLOCK_SIZE;
		offset += BLOCK_SIZE;
		++counter;
	}
	// -- After the output stream usage is done, close it
	fclose(output_file);
}

/*
 * Function: delete
 * Parameter(s): file_name - Name of the file to be deleted.
 * Description: Deletes the file with given name from MAV file system.
 */
void delete(char *file_name) {

	int counter = 0, index = -1;
	while (counter < DIRECTORY_SIZE) {
		if (!strcmp(directory.files[counter].file_name, file_name)) {
			index = counter;
			break;
		}
		++counter;
	}
	if (index < 0) {
		print_message("del error: File not found.");
		return;
	}

	counter = 0;
	int block_num = directory.files[index].blocks[counter];
	// -- clear out the blocks used by the Inode entry
	while (block_num > -1) {

		file_data[block_num][0] = '\0';
		used_space[block_num] = UNSET;
		directory.files[index].blocks[counter] = UNSET;
		block_num = directory.files[index].blocks[++counter];
	}
	directory.files[index].size = 0;
	directory.files[index].time_created = 0;
	directory.files[index].used = UNSET;
	directory.files[index].file_name[0] = '\0';
}

/*
 * Function: list
 * Description: Lists files and its details in the MAV file system.
 */
void list() {
	int counter = 0, list_counter = 0;
	struct tm *time_info;
	char timeString[15];
	Inode file;

	// -- Iterate the directory array and list the valid entries.
	while (counter < DIRECTORY_SIZE) {
		if (directory.files[counter].used == SET) {

			file = directory.files[counter];
			time_info = localtime(&file.time_created);
			strftime(timeString, sizeof(timeString), "%b %d %R", time_info);
			printf("%5lu %s %s\n", file.size, timeString, file.file_name);
			fflush(NULL);
			++list_counter;
		}
		++counter;
	}
	if (list_counter < 1) {
		printf("list: No files found.\n");
		fflush(NULL);
	}
}

/*
 * Function: disk_availability
 * Parameter(s): print - flag which decides to print the disk availability
 * to the screen.
 * Returns: The size of available disk space.
 * Description: Calculates the free space available in the MAV file system.
 */
long disk_availability(short print) {
	int counter = 0;
	unsigned long free_size = 0;
	while (counter < TOTAL_BLOCKS) {
		if (used_space[counter] == 0) {
			free_size += BLOCK_SIZE;
		}
		++counter;
	}
	if (print) {
		printf("%lu bytes free.\n", free_size);
		fflush(NULL);
	}
	return free_size;
}

/*
 * Function: get_new_file_entry
 * Returns: An index of free Inode entry available in the directory array.
 * Description: Checks if a free entry is available for a new file.
 */
int get_new_file_entry() {
	int counter = 0, index = -1;
	while (counter < DIRECTORY_SIZE) {
		if (!directory.files[counter].used) {
			index = counter;
			break;
		}
		++counter;
	}
	return index;
}

/*
 * Function: get_free_block
 * Returns: An index of free block available in the disk block array.
 * Description: Checks if a free disk block is available to store a new file.
 */
int get_free_block() {
	int counter = 0, index = -1;
	while (counter < TOTAL_BLOCKS) {
		if (used_space[counter] == UNSET) {
			index = counter;
			break;
		}
		++counter;
	}
	return index;
}

/*
 * Function: main
 * Returns: exit status of the program
 * Description: The main controller of the whole program,
 * connects with other functions and accomplishes the given task.
 */
void show_prompt(short new_line) {
	if (new_line) {
		printf("\n%s", PROMPT);
	} else {
		printf("%s", PROMPT);
	}
	fflush(NULL);
}

/*
 * Function: print_message
 * Description: Util function to print formatted messages.
 */
void print_message(char *message) {
	printf("%s%s\n", PROMPT, message);
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

	//-- Array to store tokens have a predefined size. If there aren't
	//-- enough tokens to fill up the array, left space is filled up with NULL
	while (counter < ARGS_SUPPORTED) {
		args[counter] = NULL;
		++counter;
	}
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
