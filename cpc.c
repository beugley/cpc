/*
** cpc.c
** Concurrent Process Controller
** Brian Eugley
**
** Usage: cpc -c command -d data_set_file -n num_instances [-o child-switches]
**
** This program will execute a user-specified number of concurrent instances
** of a process.  The arguments will specify the number of concurrent
** instances,  the process name, and the data set.  The process will be
** executed once for each data subset in the data set.  This program will
** keep the user-specified number of instances of the process running
** concurrently.  As each one completes, a new one will be created.  The
** output from the child processes will be handled by redirecting it to
** temporary files.  If this was not done, the output from all children would
** become mingled as they each would try to write to stdout or stderr
** simultaneously.  Instead, each writes to its own temporary files.  CPC
** will then concatenate the output from each child process by order of
** completion.
**
** When editing with vi, :set tabstop=3
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "cpc.h"

char command[COMMAND_LEN+1]="";      // receives command line argument
char dataSetFile[DATASET_LEN+1]="";  // receives command line argument
char optargs[OPTARGS_LEN+1]="";      // receives command line argument
int numInstances=0;                  // receives command line argument
info_st *pInfo = NULL;               // pointer to head of process info array
FILE *fpDS;                          // data set file pointer


int main(int argc, char *argv[])
{
char functionName[]="main";
int status = SUCCESS;          // execution status
int processCount;              // total number of child processes
int childStatus = 0;           // execution status of children
int infoIndex;                 // index into process info array
char dataSubset[200];          //
char switches[SWITCHES_LEN+1]; // local copy of switches
char *pArgList[100];           // child process arguments


if ((status = checkArgs(argc, argv)) == SUCCESS)
	{
	status = initialize();
	processCount = 0;

	/*
	** Launch a child process for each line in the data set file.
	*/
	while (status == SUCCESS  &&  fgets(dataSubset, 200, fpDS))
		{
		/*
		** If the maximum number of concurrent processes are already
		** running:
		** 1) wait for one to complete.
		** 2) decrement the active process count.
		**
		** The index into the process info array will be returned
		** by the waitForChild() procedure.  We will use the same index
		** for the next child.
		**
		** All child statuses are logically "ORed" together.  The result
		** is that this application will succeed (exit code is 0) only
		** if all of its children succeed.
		*/
		if (processCount == numInstances)
			{
			childStatus |= waitForChild(&infoIndex);
			processCount--;
			}
		else
			{
			infoIndex = processCount;
			}

		/*
		** Make the argument list and create another child process.
		** First, make a copy of the switches because makeArgList will
		** modify the contents of that argument.
		*/
		sprintf(switches, "%s %s\n", dataSubset, optargs);
		makeArgList(switches, pArgList);
		if ((status = createChild(infoIndex, pArgList)) == SUCCESS)
			{
			processCount++;
			}
		}

	/*
	** All child processes have been launched.  Wait for those which
	** have not yet completed.
	*/
	while (processCount > 0)
		{
		childStatus |= waitForChild(&infoIndex);
		processCount--;
		}

	cleanUp();
	}

printf("Exiting with status %d\n", status | childStatus);
return(status | childStatus);
}


/*
** Print the current time in the format YYYY-MM-DD:HH24:MI:SS
*/
void printTime()
{
time_t currTime;
struct tm *tmCurrTime;
char stringTime[100];

currTime = time(NULL);
tmCurrTime = localtime(&currTime);
strftime(stringTime, sizeof(stringTime)-1, "%Y-%m-%d:%T", tmCurrTime);

printf("%s\n", stringTime);
}


/*
** This procedure will check for the correct number and length of arguments.
** It will also validate the command, data_set_file, and num_instances
** arguments.
*/
int checkArgs(int argc, char *argv[])
{
char functionName[]="checkArgs";
char usage[1000];
int c;
extern char *optarg;
extern int optind, opterr, optopt;
char numInstancesArg[100]="";
char msgString[100];
struct stat fileStat;
int status = SUCCESS;

sprintf(usage, "Usage: %s -c command -d data_set_file -n num_instances"
	" [-o optional args]", argv[0]);

/*
** NOTE: getopt() appears to have a defect in HP-UX which causes it to not
** return ":" for a missing argument.  So, the following code includes
** additional logic to handle missing arguments.
*/
while ((c = getopt(argc, argv, ":d:c:n:o:")) != -1)
	{
	//printf("%c: '%s', %d, %d, %d\n", c, optarg, optind, opterr, optopt);
	switch (c)
		{
		case 'd':
			if (optarg[0] == '-')
				{
				// switch has no argument, so back-up one.
				optind--;
				}
			else if (strlen(optarg) > DATASET_LEN)
				{
				fprintf(stderr, "ERROR: data_set_file argument exceeds maximum "
					"length of %d characters!\n", DATASET_LEN);
				status = ARG_ERROR;
				}
			else
				{
				strcpy(dataSetFile, optarg);
				}
			break;
		case 'c':
			if (optarg[0] == '-')
				{
				// switch has no argument, so back-up one.
				optind--;
				}
			else if (strlen(optarg) > COMMAND_LEN)
				{
				fprintf(stderr, "ERROR: command argument exceeds maximum "
					"length of %d characters!\n", COMMAND_LEN);
				status = ARG_ERROR;
				}
			else
				{
				strcpy(command, optarg);
				}
			break;
		case 'n':
			if (optarg[0] == '-')
				{
				// switch has no argument, so back-up one.
				optind--;
				fprintf(stderr, usage);
				status = ARG_ERROR;
				}
			else
				{
				strcpy(numInstancesArg, optarg);
				numInstances = atoi(optarg);
				}
			break;
		case 'o':
			if (optind != argc)
				{
				fprintf(stderr, "ERROR: optional arguments must be last\n");
				status = ARG_ERROR;
				}
			else if (strlen(optarg) > OPTARGS_LEN)
				{
				fprintf(stderr, "ERROR: optional arguments exceed maximum "
					"length of %d characters!\n", OPTARGS_LEN);
				status = ARG_ERROR;
				}
			else
				{
				strcpy(optargs, optarg);
				}
			break;
		// All other failures should cause the usage message to be printed.
		case ':':
		case '?':
		default:
			fprintf(stderr, usage);
			status = ARG_ERROR;
			break;
		}
	}

/*
** Verify that all mandatory arguments have values.
*/
if (status == SUCCESS)
	{
	if (strlen(dataSetFile) == 0  ||  strlen(command) == 0  ||
		 strlen(numInstancesArg) == 0)
		{
		fprintf(stderr, "%s\n", usage);
		status = ARG_ERROR;
		}
	else if (numInstances <= 0)
		{
		fprintf(stderr, "ERROR: instance count must be greater than 0!\n");
		status = ARG_ERROR;
		}
	}

/*
** Display argument values if an error occurred.
*/
if (status == ARG_ERROR)
	{
	fprintf(stderr, "  -c argument is '%s'\n", command);
	fprintf(stderr, "  -d argument is '%s'\n", dataSetFile);
	fprintf(stderr, "  -n argument is '%s'\n", numInstancesArg);
	fprintf(stderr, "  -o argument is '%s'\n", optargs);
	}

/*
** Validate the data_set_file and command arguments.
*/
if (status == SUCCESS)
	{
	if (stat((const char *)dataSetFile, &fileStat))
		{
		sprintf(msgString, "ERROR: stat() failed on file '%s'", dataSetFile);
		perror(msgString);
		status = SYS_ERROR;
		}

	if (stat((const char *)command, &fileStat))
		{
		sprintf(msgString, "ERROR: stat() failed on file '%s'", command);
		perror(msgString);
		status = SYS_ERROR;
		}
	}

return status;
}


/*
** This procedure will allocate memory for the process information array.
** It will also fetch the batch date and verify that there is not another
** instance of Task Manager running that is attempting to process the same
** job.  If everything is okay, then addToRunLog() will be called.
*/
int initialize()
{
int status = SUCCESS;
char functionName[]="initialize";

if (!(pInfo = (info_st *)malloc(numInstances * sizeof(info_st))))
	{
	fprintf(stderr, "ERROR: couldn't allocate process array\n");
	status = MEM_ERROR;
	}
   
if (!(fpDS = fopen((const char *)dataSetFile, "r")))
	{
	fprintf(stderr, "ERROR: couldn't open file %s\n", dataSetFile);
	status = IO_ERROR;
	}

return status;
}


/*
** This procedure will create an argument list from the argument line.
** execvp() expects a pointer to an array of pointers to arguments.  The
** first element must be the program name.  Subsequent elements are the
** actual arguments.  The last element must be a NULL pointer.
*/
void makeArgList(char *switches, char **pArgList)
{
int argCount;              // count of arguments in list
char *token;               // pointer to next token in argument line

/*
** The first element in the list must be the program name itself.
*/
pArgList[0] = command;
argCount = 1;

/*
** Tokens are delimited by spaces.
*/
token = strtok(switches, " \n");

/*
** Add each token to the argument list.
*/
while (token)
	{
	pArgList[argCount] = token;
	argCount++;
	token = strtok('\0', " \n");
	}

/*
** Terminate the argument list with a NULL pointer.
*/
pArgList[argCount] = '\0';
}


/*
** This procedure will fork and exec a new child process.
*/
int createChild(int infoIndex, char **pArgList)
{
int status;                  // execution status
pid_t childpid;              // child process id
char functionName[]="createChild";

/*
** Get names for the temporary files and save them in the info structure.
*/
pInfo[infoIndex].outFile = tempnam(NULL, NULL);
pInfo[infoIndex].errFile = tempnam(NULL, NULL);
if (!pInfo[infoIndex].outFile  ||  !pInfo[infoIndex].errFile)
	{
	fprintf(stderr, "tempnam returned NULL\n");
	return SYS_ERROR;
	}

/*
** Create the child process.
*/
childpid = fork();
switch (childpid)
	{
	case -1: /* Something went wrong! */
		perror("ERROR: fork() failed");
		status = SYS_ERROR;
		break;
	case 0: /* This is the child! */
		/*
		** Set up the temp files.
		** stdout and stderr will each be redirected to one of those files.
		*/
		if ((status = tempFiles(&pInfo[infoIndex])) == SUCCESS)
			{
			/*
			** Change the child into the program specified on the command line.
			** Unless there is an error, execvp will NOT return!
			*/
			execvp(pArgList[0], pArgList);
			/*
			** The remainder of this case should NEVER execute!
			*/
			perror("ERROR: execvp() failed");
			status = SYS_ERROR;
			}
		/*
		** Something went wrong.  The child must not continue!
		** Terminate it now!
		*/
		exit(status);
		break;
	default: /* This is the parent! */
		/*
		** Display the child's pid and argument list.
		*/
		printTime();
		printf("Spawned child %d, program:", childpid);
		while (*pArgList)
			{
			printf(" %s", *pArgList);
			pArgList++;
			}
		printf("\n");

		/*
		** Save the child's pid in the info structure.
		*/
		pInfo[infoIndex].pid = childpid;

		status = SUCCESS;
		break;
	}

return status;
}


/*
** This procedure will wait until a child completes.  It will then get the
** execution status of that child and get the output from the temporary files.
*/
int waitForChild(int *pinfoIndex)
{
pid_t childpid;             /* child process id */
int childStatus;            /* child execution status */
int status;                 /* execution status */

childpid = wait(&childStatus);
status = getChildStatus(childStatus, childpid);

/*
** Get the output from the temporary files and send to the output of this
** application.
*/
getOutput(pinfoIndex, childpid);

return status;
}


/*
** This procedure will get the execution status of a child process.
*/
int getChildStatus(int childStatus, pid_t childpid)
{
int status;                  /* execution status */
char functionName[]="getChildStatus";

/*
** Did the child end because of a call to exit()?
*/
if (WIFEXITED(childStatus))
	{
	/*
	** Get the exit code from the child.
	*/
	status = WEXITSTATUS(childStatus);
	printTime();
	printf("child %d exited with status %d\n", childpid, status);
	}
/*
** Did the child terminate because of a signal?
*/
else if (WIFSIGNALED(childStatus))
	{
	/*
	** Use the signal number as an error code.
	*/
	status = WTERMSIG(childStatus);
	printf("child %d terminated due to signal %d\n", childpid, status);
	}
else
	{
	status = OTHER_ERROR;
	printf("child %d terminated for an unknown reason\n", childpid);
	}

return status;
}


/*
** This procedure will associate stdout and stderr with temporary files for
** the child processes.
** Only the child processes execute this routine!
*/
int tempFiles(info_st *pInfo)
{
int status;                /* execution status */
int outFileDes;            /* file descriptor for stdout */
int errFileDes;            /* file descriptor for stderr */
char functionName[]="tempFiles";

/*
** Create the files for stdout and stderr.
*/
if ((outFileDes = open(pInfo->outFile, O_WRONLY | O_CREAT | O_EXCL,
	S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) != -1  &&
	(errFileDes = open(pInfo->errFile, O_WRONLY | O_CREAT | O_EXCL,
	S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) != -1)
	{
	/*
	** "Redirect" the output from stdout and stderr to the temp files.
	*/
	if (dup2(outFileDes, STDOUT_FILENO) != -1  &&
		dup2(errFileDes, STDERR_FILENO) != -1)
		{
		/*
		** Close the file descriptors which point to the temp files.
		** The stdout and stderr descriptors now point to the temp files.
		*/
		if (close(outFileDes) == 0  &&  close(errFileDes) == 0)
			{
			status = SUCCESS;
			}
		else
			{
			status = IO_ERROR;
			fprintf(stderr, "ERROR: Couldn't close file descriptors!\n");
			}
		}
	else
		{
		status = SYS_ERROR;
		fprintf(stderr, "ERROR: Couldn't duplicate stdout/stderr descriptors!\n");
		}
	}
else
	{
	status = IO_ERROR;
	fprintf(stderr, "ERROR: Couldn't create temporary output files!\n");
	}

return status;
}


/*
** This procedure will get the output from a completed child process.  The
** files will be copied to the parent's stdout and stderr.  This must be done
** because multiple children are executing concurrently.  If they all
** wrote directly to stdout and stderr, their output would become mingled.
** By redirecting the children's output to files, the parent can read those
** files after each child has completed.  The parent can then control the
** output of the children and release it serially to its own output.
*/
void getOutput(int *pinfoIndex, pid_t childpid)
{
char command[1000];   /* command to execute via system() */

/*
** Find the element in the info structure for this child.
*/
*pinfoIndex = 0;
while (pInfo[*pinfoIndex].pid != childpid)
	{
	(*pinfoIndex)++;
	}

/*
** Flush stdout (this is the parent) and then cat the child's stdout output.
** It will go to the parent's stdout.
*/
printf("**************************************************************\n");
printf("******************* Stdout from child %5d ******************\n",
	childpid);
printf("**************************************************************\n");
fflush(stdout);
strcpy(command, "cat ");
strcat(command, pInfo[*pinfoIndex].outFile);
system(command);
printf("**************************************************************\n");
printf("***************** End stdout from child %5d ****************\n",
	childpid);
printf("**************************************************************\n\n\n");
fflush(stdout);

/*
** Flush stderr (this is the parent) and then cat the child's stderr output.
** It will go to the parent's stderr.
*/
fprintf(stderr, "\n*******************************************************\n");
fprintf(stderr, "*************** Stderr from child %5d ***************\n",
	childpid);
fprintf(stderr, "*******************************************************\n");
fflush(stderr);
strcpy(command, "cat ");
strcat(command, pInfo[*pinfoIndex].errFile);
strcat(command, " >&2");
system(command);
fprintf(stderr, "*******************************************************\n");
fprintf(stderr, "************* End stderr from child %5d *************\n",
	childpid);
fprintf(stderr, "*******************************************************\n\n");
fflush(stderr);

/*
** Remove the child's temporary files and free the memory allocated by tempnam.
*/
unlink(pInfo[*pinfoIndex].outFile);
unlink(pInfo[*pinfoIndex].errFile);
free(pInfo[*pinfoIndex].outFile);
free(pInfo[*pinfoIndex].errFile);

return;
}


/*
** Cleanup prior to termination.
*/
void cleanUp()
{
if (pInfo)
	{
	free(pInfo);
	}

fclose(fpDS);
}

