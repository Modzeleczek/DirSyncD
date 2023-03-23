#ifndef DIRSYNCD_H
#define DIRSYNCD_H

#include <dirent.h>
#include <sys/stat.h>

/*
Analyzes options and arguments passed to the program and in its parameters
  writes values ready for use.
reads:
argc - number of program parameters (options and arguments together)
argv - programu parameters
writes:
source - source directory path
destination - target directory path
interval - sleep time in seconds
recursive - recursive directory synchronization (boolean)
threshold - minimal file size to consider it big (this function stores threshold
  in a global variable)
returns:
< 0 if an error occured
0 if no error occured
*/
int parseParameters(int argc, char **argv, char **source, char **destination,
  unsigned int *interval, char *recursive);

/*
Handles signal SIGUSR1.
reads:
signo - number of the handled signal - always SIGUSR1
*/
void sigusr1Handler(int signo);
/*
Handles signal SIGTERM.
reads:
signo - number of the handled signal - always SIGTERM
*/
void sigtermHandler(int signo);

/*
Starts a child process from the parent process. Stops the parent process.
  Transforms the child process into a daemon.
  Sleeps and synchronizes directories. Handles signals.
reads:
source - source directory path
destination - target directory path
interval - sleep time in seconds
recursive - recursive directory synchronization (boolean)
*/
void runDaemon(char *source, char *destination, unsigned int interval,
  char recursive);

#endif // DIRSYNCD_H
