/*
** cpc.h
** Concurrent Process Controller
** Brian Eugley
*/

#ifndef _INCLUDE_CPC_
#define _INCLUDE_CPC_

/*
** Status codes
*/
#define SUCCESS 0
#define ARG_ERROR 1
#define MEM_ERROR 2
#define SYS_ERROR 3
#define IO_ERROR 4
#define OTHER_ERROR 5

/*
** Argument lengths
*/
#define DATASET_LEN 100
#define COMMAND_LEN 200
#define SWITCHES_LEN 300
#define OPTARGS_LEN 200

/*
** Process information structure.
*/
typedef struct
	{
	pid_t pid;                  // process id of the child
	char *outFile;              // name of the stdout file
	char *errFile;              // name of the stderr file
	} info_st;

/*
** Function prototypes
*/
void printTime();
int checkArgs(int, char **);
int initialize();
int waitForChild(int *);
int getChildStatus(int, pid_t);
void getOutput(int *, pid_t);
void makeArgList(char *, char **);
int createChild(int, char **);
int tempFiles(info_st *);
void cleanUp();

#endif // _INCLUDE_CPC_
