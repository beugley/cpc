cpc
===

Concurrent Process Controller


Usage: cpc -c command -d data_set_file -n num_instances [-o child-switches]

This program will execute a user-specified number of concurrent instances of a process.

The arguments will specify the number of concurrent instances,  the process name, and the data set.  The process will be executed once for each data subset in the data set.  This program will keep the user-specified number of instances of the process running concurrently.  As each one completes, a new one will be created.  The output from the child processes will be handled by redirecting it to temporary files.  If this was not done, the output from all children would become mingled as they each would try to write to stdout or stderr simultaneously.  Instead, each writes to its own temporary files.  CPC will then concatenate the output from each child process by order of completion.
