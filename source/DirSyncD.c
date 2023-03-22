#include "directory.h"
#include "DirSyncD.h"
#include "path.h"
#include "synchronization.h"

#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <signal.h>
#include <errno.h>
#include <syslog.h>

/*
Essential arguments:
- source_path - path of the directory from which we copy
- target_path - path of the directory to which we copy

Additional options:
- -i <sleep_time> - sleep time
- -R - recursive directory synchronization
- -t <big_file_threshold> - minimal file size to consider it big

Usage:
DirSyncD [-i <sleep_time>] [-R] [-t <big_file_threshold>] source_path target_path

Send signal SIGUSR1 to the daemon:
- during sleep - to prematurely wake it up.
- during synchronization - to force it to repeat the synchronization immediately after finishing the current one.

Send signal SIGTERM to the daemon:
- during sleep - to stop it.
- during synchronization - to force it to stop after finishing the current synchronization unless the daemon receives SIGUSR1 during it.
*/
int main(int argc, char **argv)
{
  char *source, *destination;
  unsigned int interval;
  char recursive;
  // Analyze (parse) parameters passed on program start. If an error occured
  if (parseParameters(argc, argv, &source, &destination, &interval, &recursive) < 0)
  {
    // Print the correct way of using the program.
    printf("Usage: DirSyncD [-i <sleep_time>] [-R] [-t <big_file_threshold>] source_path target_path\n");
    // Stop the parent process.
    return -1;
  }
  // Check if the source directory is valid. If it is invalid
  if (directoryValid(source) < 0)
  {
    // Print the error message for error code stored in errno variable. 
    perror(source);
    // Stop the parent process.
    return -2;
  }
  // Check if the target directory is valid. If it is invalid
  if (directoryValid(destination) < 0)
  {
    // Print the error message for error code stored in errno variable. 
    perror(destination);
    // Stop the parent process.
    return -3;
  }

  // Start the daemon.
  runDaemon(source, destination, interval, recursive);

  return 0;
}

// Without 'static' because this global variable is used in other .c files.
// Big file threshold. If the file size is lesser than threshold, then during copying the file is considered small, otherwise big.
unsigned long long threshold;

int parseParameters(int argc, char **argv, char **source, char **destination, unsigned int *interval, char *recursive)
{
  // If no parameters were passed
  if (argc <= 1)
    // Return error code.
    return -1;
  // Save default sleep time equal to 5*60 s = 5 min.
  *interval = 5 * 60;
  // Save default non-recursive directory synchronization.
  *recursive = 0;
  // Save default big file threshold equal to maximal possible value of unsigned long long int variable.
  threshold = ULLONG_MAX;
  int option;
  // Place ':' at the beginning of __shortopts to distinguish between '?' (unknown option) and ':' (no value given for an option).
  while ((option = getopt(argc, argv, ":Ri:t:")) != -1)
  {
    switch (option)
    {
    case 'R':
      // Enable recursive directory synchronization.
      *recursive = (char)1;
      break;
    case 'i':
      // String optarg is sleep time in seconds. Transform it into unsigned int. If sscanf did not correctly fill interval, the passed time value has invalid format and
      if (sscanf(optarg, "%u", interval) < 1)
        // Return error code.
        return -2;
      break;
    case 't':
      // String optarg is big file threshold. Transform it into unsigned long long int. If sscanf did not correctly fill THRESHOLD, the passed file size value has invalid format and
      if (sscanf(optarg, "%llu", &threshold) < 1)
        // Return error code.
        return -3;
      break;
    case ':':
      // If option -i or -t was passed without its value, print message
      printf("Option demands a value\n");
      // Return error code.
      return -4;
      break;
    case '?':
      // If option other than -R, -i, -t was specified
      printf("Unknown option: %c\n", optopt);
      // Return error code.
      return -5;
      break;
    default:
      // If getopt returned a value other than listed above (it should never happen)
      printf("Unknown error");
      // Return error code.
      return -6;
      break;
    }
  }
  // Count the arguments not being options (should be exactly 2: source and target paths).
  int remainingArguments = argc - optind;
  if (remainingArguments != 2) // If there are not exactly 2 arguments
    // Return error code.
    return -7;
  // Optind is index of the first argument not being an option parsed by getopt. Therefore, optind should be index of source path argument. Save the source path.
  *source = argv[optind];
  // Save the target path.
  *destination = argv[optind + 1];
  // Return the correct ending code.
  return 0;
}

// Flag of forced synchronization set in SIGUSR1 signal handler function.
char forcedSynchronization;
// SIGUSR1 signal handler function.
void sigusr1Handler(int signo)
{
  // Set the flag of forced synchronization.
  forcedSynchronization = 1;
}

// Flag of stopping set in SIGTERM signal handler function.
char stop;
// SIGTERM signal handler function.
void sigtermHandler(int signo)
{
  // Set the flag of stopping.
  stop = 1;
}

void runDaemon(char *source, char *destination, unsigned int interval, char recursive)
{
  // Create a child process.
  pid_t pid = fork();
  // If an error occured during fork execution still in the parent process, so no child process was created.
  if (pid == -1)
  {
    // Print the error message for error code stored in errno variable. 
    perror("fork");
    // Stop the parent process with an error status code.
    exit(-1);
  }
  // In the parent process, pid variable has value equal to PID of the created child process.
  else if (pid > 0)
  {
    // Being still in the parent process, print child process' PID.
    printf("PID of the child process: %i\n", pid);
    // Stop the parent process with a status code indicating no errors.
    exit(0);
  }
  // The code below is executed in the child process because pid == 0 in it. Transform the child process into a daemon (Robert Love - "Linux System Programming", page 177, at least in Polish version of the book). Initially, set status code indicating no error.
  int ret = 0;
  char *sourcePath = NULL, *destinationPath = NULL;
  // In ext4 file system, an absolute path can be at most PATH_MAX (4096) bytes long. Reserve PATH_MAX bytes for the source directory path. If an error occured
  if ((sourcePath = malloc(sizeof(char) * PATH_MAX)) == NULL)
    // Set status code indicating an error. After that, the program immediately goes to the end of the current function.
    ret = -1;
  // Reserve PATH_MAX bytes for the target directory path. If an error occured
  else if ((destinationPath = malloc(sizeof(char) * PATH_MAX)) == NULL)
    // Set status code indicating an error.
    ret = -2;
  // Create the absolute source directory path. If an error occured
  else if (realpath(source, sourcePath) == NULL)
  {
    // Print the error message for error code stored in errno variable. It is still possible because we have not readdressed child process' descriptors and we can access stdout.
    perror("realpath; source");
    // Set status code indicating an error.
    ret = -3;
  }
  // Create the absolute target directory path. If an error occured
  else if (realpath(destination, destinationPath) == NULL)
  {
    // Print the error message for error code stored in errno variable.
    perror("realpath; destination");
    // Set status code indicating an error.
    ret = -4;
  }
  // Create a new session and process group. If an error occured
  else if (setsid() == -1)
    // Set status code indicating an error.
    ret = -5;
  // Set process' current working directory to "/". If an error occured
  else if (chdir("/") == -1)
    // Set status code indicating an error.
    ret = -6;
  else
  {
    int i;
    // Essentially close stdin, stdout, stderr (descriptors: 0, 1, 2).
    for (i = 0; i <= 2; ++i)
      // If an error occured
      if (close(i) == -1)
      {
        // Set status code indicating an error.
        ret = -(50 + i);
        break;
      }
  }
  // If no error has occured yet
  if (ret >= 0)
  {
    int i;
    // If greater descriptors (from 3 to 1023) are open, then close them because in Linux a process can have max 1024 open descriptors.
    for (i = 3; i <= 1023; ++i)
      // Close i-th descriptor. If an error occured, ignore it.
      close(i);
    sigset_t set;
    // Readdress descriptors 0, 1, 2 to '/dev/null'. Set descriptor 0 (stdin, the least from the closed descriptors) to '/dev/null'. If an error occured
    if (open("/dev/null", O_RDWR) == -1)
      // Do not call perror anymore because the child process cannot print an error to the terminal which started the parent process. Set status code indicating an error.
      ret = -8;
    // Set descriptor 1 (stdout) to the same as descriptor 0 ('/dev/null'). If an error occured
    else if (dup(0) == -1)
      // Set status code indicating an error.
      ret = -9;
    // Set descriptor 2 (stderr) to the same as descriptor 0 ('/dev/null'). If an error occured
    else if (dup(0) == -1)
      // Set status code indicating an error.
      ret = -10;
    // Here, the child process is already a daemon. Register SIGUSR1 signal handler function. If an error occured
    else if (signal(SIGUSR1, sigusr1Handler) == SIG_ERR)
      // Set status code indicating an error.
      ret = -11;
    // Register SIGTERM signal handler function. If an error occured
    else if (signal(SIGTERM, sigtermHandler) == SIG_ERR)
      // Set status code indicating an error.
      ret = -12;
    // Initialize an empty signal set. If an error occured
    else if (sigemptyset(&set) == -1)
      // Set status code indicating an error.
      ret = -13;
    // Add SIGUSR1 to the signal set. If an error occured
    else if (sigaddset(&set, SIGUSR1) == -1)
      // Set status code indicating an error.
      ret = -14;
    // Add SIGTERM to the signal set. If an error occured
    else if (sigaddset(&set, SIGTERM) == -1)
      // Set status code indicating an error.
      ret = -15;
    else
    {
      // Calculate source directory absolute path length.
      size_t sourcePathLength = strlen(sourcePath);
      // If there is no '/' immediately before '\0' (null terminator)
      if (sourcePath[sourcePathLength - 1] != '/')
        // Insert '/' in place of '\0'. Increment path length by 1. Insert'\0' after '/'.
        stringAppend(sourcePath, sourcePathLength++, "/");
      // Calculate target directory absolute path length.
      size_t destinationPathLength = strlen(destinationPath);
      // If there is no '/' immediately before'\0'
      if (destinationPath[destinationPathLength - 1] != '/')
        // Insert '/' in place of '\0'. Increment path length by 1. Insert '\0' after '/'.
        stringAppend(destinationPath, destinationPathLength++, "/");
      synchronizer synchronize;
      // If non-recursive synchronization is set
      if (recursive == 0)
        // Save a pointer to function synchronizing non-recursively.
        synchronize = synchronizeNonRecursively;
      // If recursive synchronization is set
      else
        // Save a pointer to function synchronizing recursively.
        synchronize = synchronizeRecursively;
      // Initially, set to 0 because the variable is used to stop the daemon (break the loop) with signal SIGTERM.
      stop = 0;
      // Initially, set to 0 because the variable is used to skip the sleep start with signal SIGUSR1.
      forcedSynchronization = 0;
      while (1)
      {
        // If any synchronization was not forced with signal SIGUSR1
        if (forcedSynchronization == 0)
        {
          // Open connection to log ('/var/log/syslog').
          openlog("DirSyncD", LOG_ODELAY | LOG_PID, LOG_DAEMON);
          // In the log, write a message about sleep start.
          syslog(LOG_INFO, "falling asleep");
          // Close the connection to the log.
          closelog();
          // Put the daemon to sleep.
          unsigned int timeLeft = sleep(interval);
          // Open the connection to the log.
          openlog("DirSyncD", LOG_ODELAY | LOG_PID, LOG_DAEMON);
          // In the log, write a message about waking up with sleep time in seconds.
          syslog(LOG_INFO, "waking up; slept for %u s", interval - timeLeft);
          // Close the connection to the log.
          closelog();
          // If sleep was interrupted by receiving SIGTERM
          if (stop == 1)
            // Break the loop.
            break;
        }
        // Start blocking signals from the set (SIGUSR1 and SIGTERM). If an error occured
        if (sigprocmask(SIG_BLOCK, &set, NULL) == -1)
        {
          // Set status code indicating an error.
          ret = -16;
          // Break the loop.
          break;
        }
        // Start synchronizing with the selected function. Ignore errors but write the status code to the log. 0 means that the entire synchronization went without errors. Value different from 0 means that directories may be not fully synchronized.
        int status = synchronize(sourcePath, sourcePathLength, destinationPath, destinationPathLength);
        // Open the connection to the log.
        openlog("DirSyncD", LOG_ODELAY | LOG_PID, LOG_DAEMON);
        // In the log, write a message about finishing the synchronization with status code.
        syslog(LOG_INFO, "finishing synchronization; %i", status);
        // Close the connection to the log.
        closelog();
        // Regardless of whether the synchronization was forced or automatic (after sleeping for the entire sleep time), set 0 after its finish.
        forcedSynchronization = 0;
        // Stop blocking signals from the set (SIGUSR1 and SIGTERM). If an error occured
        if (sigprocmask(SIG_UNBLOCK, &set, NULL) == -1)
        {
          // Set status code indicating an error.
          ret = -17;
          // Break the loop.
          break;
        }
        // If during the synchronization SIGUSR1 or SIGTERM was received, then after stopping blocking them, their handler functions are executed.
        // If during the synchronization SIGUSR1 was received, then immediately after its finish, a next synchronization is performed.
        // If during the synchronization SIGUSR1 was not received but SIGTERM was, then after its finish, the daemon stops.
        // If during the synchronization SIGUSR1 and SIGTERM were received, then after its finish, a next synchronization is performed. If during it SIGUSR1 is not received, then after its finish, the daemon stops. Otherwise a third synchronization is performed, etc. After finishing the first synchronization in the series during which SIGUSR1 is not received, the daemon stops.
        // If no next synchronization was forced but stopping was
        if (forcedSynchronization == 0 && stop == 1)
          // Break the loop.
          break;
      }
    }
  }
  // If an error occured somewhere, go here.
  // If memory for source directory path was reserved
  if (sourcePath != NULL)
    // Release memory.
    free(sourcePath);
  // If memory for target directory path was reserved
  if (destinationPath != NULL)
    // Release memory.
    free(destinationPath);
  // Open connection to the log.
  openlog("DirSyncD", LOG_ODELAY | LOG_PID, LOG_DAEMON);
  // In the log, write a message about daemon stop with status code.
  syslog(LOG_INFO, "stopping; %i", ret);
  // Close the connection to the log.
  closelog();
  // Stop the daemon process.
  exit(ret);
}
