/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Author: Jonathan Pennington                                           *
 *                                                                       *
 * isSame.c: A way to check to see if two files, or a file and a         *
 * checksum are the same                                                 *
 * Version: 0.05                                                         *
 *                                                                       *
 * Also used to demonstrate pipe fork dup exec to students in CS 3240    *
 *                                                                       *
 * Takes an argument for two files, opens the files, and compares the    *
 * sha512sum of each to see if they're the same.                         *
 *                                                                       *
 * Alternate usage requires argv[1] to be --one-file. It opens file in   *
 * command line and compares it to a checksum in the command line        *
 *                                                                       *
 * This version uses piping to store the results of the sha512sum bash   *
 * command in two variables, checks them, then returns 0 if they are     *
 * the same and 1 if they are different                                  *
 *                                                                       *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "apue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>

// This is what a header guard looks like. Only define MAX_SIZE if it is not defined in the above headers
#ifndef MAX_SIZE
	#define MAX_SIZE 129	// Just enough so valgrind doesn't complain
#endif

// Uncomment to manually define DEBUG, otherwise add -DDEBUG to gcc flag when compiling
//#define DEBUG

int checkTwoFiles(char *, char *);
int checkOneFile(char *, char *);

int main(int argc, char ** argv){

	int fd[2];

	// Don't do anything if two additional arguments are not given
	if(argc<3 || (strcasecmp(argv[1],"--one-file")==0 && argc<4)){
		printf("Usage: <command> <file1> <file2>\n");
		printf("Usage: <command> <--one-file> <file> <sha512 checksum>\n");
		return -1;
	}

	// Check to make sure files exist
	if(strcasecmp(argv[1],"--one-file")!=0){
		if((fd[0]=open(argv[1],O_RDONLY))==-1) err_sys("Cannot open %s",argv[1]);
		if((fd[1]=open(argv[2],O_RDONLY))==-1) err_sys("Cannot open %s",argv[2]);
		if((close(fd[1]))==-1) err_sys("unable to close fd[1]");
	}
	else if((fd[0]=open(argv[2],O_RDONLY))==-1) err_sys("File %s does not exist",argv[2]);

	if((close(fd[0]))==-1) err_sys("unable to close fd[0]");

	// Return 0 if they're the same, return 1 if they are different
	if(strcasecmp(argv[1],"--one-file")==0) return checkOneFile(argv[2],argv[3]);
	else return checkTwoFiles(argv[1],argv[2]);
}

int checkTwoFiles(char * arg1, char * arg2){

	pid_t p1, p2;
	char* sum1=calloc(MAX_SIZE,1);
	char* sum2=calloc(MAX_SIZE,1);
	int status=0;
	int fd1[2];
	int fd2[2];

	// We will write to these piped file descriptors
	if((pipe(fd1))==-1) err_sys("Failed to pipe fd1");

	// Fork into a child process
	if((p1=fork())==-1) err_sys("Failed to fork process 1");

	// If we're the first child
	if(p1==0){
		// We don't need this door open, so we shall close it. We don't want to let the flies in
		if((close(fd1[0]))==-1) err_sys("Failed to close fd1[0] in child 1");
		// Switch standard output to fd1[1]
		if((dup2(fd1[1],STDOUT_FILENO))==-1) err_sys("Dup2 failed in child process 1");
		// Put the sha512 checksum in fd1[1]
		if(execlp("sha512sum","sha512sum",arg1,(void*)'\0')==-1) err_sys("Failed to execute binary command \"sha512sum\"");
	}

	// If we're the original parent
	else if(p1>0) {

		// Read what was written into fd1[1] above into sum1
		if((read(fd1[0],sum1,128))==-1) err_sys("Failed to read fd1[0]");

		// We will now use a different fd for child 2
		if((pipe(fd2))==-1) err_sys("Failed to pipe fd2");

		// Fork into yet another child process
		if((p2=fork())==-1) err_sys("Failed to fork process 2");

		// If we're the second child
		if(p2==0){
			// We don't need this door open, so we shall close it. We don't want to let the flies in
			if((close(fd2[0]))==-1) err_sys("Failed to close fd2[0] in child 2");
			// Switch standard output to fd2[1]
			if((dup2(fd2[1],STDOUT_FILENO))==-1) err_sys("Dup2 failed in child process 2");
			// Put the sha512 checksum in fd2[1]
			if(execlp("sha512sum","sha512sum",arg2,(void*)'\0')==-1) err_sys("Failed to execute binary command \"sha512sum\"");
			return -1; // Only happens with an error
		}

		// If we're the original parent
		else if(p2>0) {
			// Read what was written into fd2[1] above into sum2
			if((read(fd2[0],sum2,128))==-1) err_sys("Failed to read fd2[0]");
		}
	}

	// Parent waits for both children to finish their execlp
	if((waitpid(-1,&status,0))==-1) err_sys("wait error");
	if((waitpid(-1,&status,0))==-1) err_sys("wait error");

	// Print the sha512 sums for reference if debug flag is set
	#ifdef DEBUG
		printf("%s\n",sum1);
		printf("%s\n",sum2);
	#endif

	// Closing piped file descriptors
	if((close(fd1[0]))==-1) err_sys("Failed to close fd1[0] in parent");
	if((close(fd1[1]))==-1) err_sys("Failed to close fd1[1] in parent");
	if((close(fd2[0]))==-1) err_sys("Failed to close fd2[0] in parent");
	if((close(fd2[1]))==-1) err_sys("Failed to close fd2[1] in parent");

	// Let us know whether the checksums match
	if((strcmp(sum1,sum2))==0) {
		free(sum1);
		free(sum2);
		#ifdef DEBUG
			printf("Files are the same\n");
		#endif
		return 0;
	}
	else {
		free(sum1);
		free(sum2);
		#ifdef DEBUG
			printf("Files are not the same\n");
		#endif
		return 1;
	}

}

// This is a method for checking a file against a sha512 checksum
// It takes as arguments a file path and a sha512 checksum
int checkOneFile(char * arg1, char * arg2){

	pid_t p1;
	char* sum1=calloc(MAX_SIZE,1);
	int status=0;
	int fd[2];

	// Open and truncate first temporary file
	if((pipe(fd))==-1){
		err_sys("Failed to pipe fd");
	}

	// Fork into a child process
	if((p1=fork())==-1) err_sys("Failed to fork process 1");

	// If we're the child
	if(p1==0){
		// Switch standard output to fd[1]
		if((dup2(fd[1],STDOUT_FILENO))==-1) err_sys("Dup2 failed in child process 1");
		// Put the sha512 checksum in fd[1]
		if(execlp("sha512sum","sha512sum",arg1,(void*)'\0')==-1) err_sys("Failed to execute binary command \"sha512sum\"");;
	}

	// If we're the parent
	else if(p1>0){
		// Fork into a child process
		if((waitpid(-1,&status,0))==-1) err_sys("wait error");
	}
	// Read the sha512sum from fd[0]
	if((read(fd[0],sum1,128))==-1) err_sys("Failed to read from fd[0]");

	// Print for reference
	#ifdef DEBUG
		printf("%s\n",sum1);
		printf("%s\n",arg2);
	#endif

	// Report whether or not sha512sum and argv[2] match
	if((strcmp(sum1,arg2))==0) {
		free(sum1);
		#ifdef DEBUG
			printf("File matches\n");
		#endif
		return 0;
	}
	else{
		free(sum1);
		#ifdef DEBUG
			printf("File does not match\n");
		#endif
		return 1;
	}

}
