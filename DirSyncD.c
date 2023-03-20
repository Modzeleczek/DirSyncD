#include "DirSyncD.h"

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
#include <sys/mman.h>
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
        printf("sposob uzycia: DirSyncD [-i <czas_spania>] [-R] [-t <prog_duzego_pliku>] sciezka_zrodlowa sciezka_docelowa\n");
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

struct element
{
    // Next list node.
    element *next;
    // Pointer to a directory entry (file or subdirectory).
    struct dirent *entry;
};
int cmp(element *a, element *b)
{
    // Function comparing list nodes (lexicographic order).
    return strcmp(a->entry->d_name, b->entry->d_name);
}

struct list
{
    // The first and last nodes. Save a pointer to the last node to add new nodes to the list in constant time.
    element *first, *last;
    // Number of list nodes.
    unsigned int count;
};
void initialize(list *l)
{
    // Set pointers to the first and last nodes to NULL.
    l->first = l->last = NULL;
    // Set the number of nodes to 0.
    l->count = 0;
}
int pushBack(list *l, struct dirent *newEntry)
{
    element *new = NULL;
    // Reserve memory for a new list node. If an error occured
    if ((new = malloc(sizeof(element))) == NULL)
        // Return an error code.
        return -1;
    // Store the directory entry pointer in the list node 
    new->entry = newEntry;
    // Set the pointer to the next list node to NULL.
    new->next = NULL;
    // If the list is empty, thus first == NULL i last == NULL
    if (l->first == NULL)
    {
        // Set the new node as the first one
        l->first = new;
        // and the last one.
        l->last = new;
    }
    // If the list is not empty
    else
    {
        // Set the new node as the next after current last one.
        l->last->next = new;
        // Move the last node pointer to the newly added node.
        l->last = new;
    }
    // Increment the number of nodes.
    ++l->count;
    // Return the correct ending code.
    return 0;
}
void clear(list *l)
{
    // Save the pointer to the first node.
    element *cur = l->first, *next;
    while (cur != NULL)
    {
        // Save the pointer to the next node.
        next = cur->next;
        // Release the node's memory.
        free(cur);
        // Move the pointer to the next node.
        cur = next;
    }
    // Zero out the list's fields.
    initialize(l);
}
void listMergeSort(list *l)
{
    element *p, *q, *e, *tail, *list = l->first;
    int insize, nmerges, psize, qsize, i;
    if (list == NULL) // Silly special case: if `list' was passed in as NULL, return immediately.
        return;
    insize = 1;
    while (1)
    {
        p = list;
        list = NULL;
        tail = NULL;
        nmerges = 0; // count number of merges we do in this pass
        while (p)
        {
            nmerges++; // there exists a merge to be done
            // step `insize' places along from p
            q = p;
            psize = 0;
            for (i = 0; i < insize; i++)
            {
                psize++;
                q = q->next;
                if (!q)
                    break;
            }
            // if q hasn't fallen off end, we have two lists to merge
            qsize = insize;
            // now we have two lists; merge them
            while (psize > 0 || (qsize > 0 && q))
            {
                // decide whether next element of merge comes from p or q
                if (psize == 0)
                {
                    // p is empty; e must come from q.
                    e = q;
                    q = q->next;
                    qsize--;
                }
                else if (qsize == 0 || !q)
                {
                    // q is empty; e must come from p.
                    e = p;
                    p = p->next;
                    psize--;
                }
                else if (cmp(p, q) <= 0)
                {
                    // First element of p is lower (or same); e must come from p.
                    e = p;
                    p = p->next;
                    psize--;
                }
                else
                {
                    // First element of q is lower; e must come from q.
                    e = q;
                    q = q->next;
                    qsize--;
                }
                // add the next element to the merged list
                if (tail)
                {
                    tail->next = e;
                }
                else
                {
                    list = e;
                }
                tail = e;
            }
            // now p has stepped `insize' places along, and q has too
            p = q;
        }
        tail->next = NULL;
        // If we have done only one merge, we're finished.
        if (nmerges <= 1) // allow for nmerges==0, the empty list case
        {
            l->last = tail;
            l->first = list;
            return;
        }
        // Otherwise repeat, merging lists twice the size
        insize *= 2;
    }
}

// static - global variable visible only in the current file below its declaration
// Big file threshold. If the file size is lesser than threshold, then during copying the file is considered small, otherwise big.
static unsigned long long threshold;
// Size of the buffer used to copy files.
#define BUFFERSIZE 4096

void stringAppend(char *dst, const size_t offset, const char *src)
{
    // Move offset bytes from the beginning of string dst.  Starting at the calculated position, insert string src.
    strcpy(dst + offset, src);
    // Strcat can be used but it appends src at the end of dst and presumably wastes time calculating the length of dst using strlen.
}
size_t appendSubdirectoryName(char *path, const size_t pathLength, const char *subName)
{
    // Append the subdirectory name to source directory path.
    stringAppend(path, pathLength, subName);
    // Calculate the length of subdirectory path as sum of source directory path length and subdirectory name length.
    size_t subPathLength = pathLength + strlen(subName);
    // Append '/' to the created subdirectory path.
    stringAppend(path, subPathLength, "/");
    // Increment subdirectory path length by 1 because '/' was appended.
    subPathLength += 1;
    // Return created subdirectory path length.
    return subPathLength;
}

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
            printf("opcja wymaga podania wartosci\n");
            // Return error code.
            return -4;
            break;
        case '?':
            // If option other than -R, -i, -t was specified
            printf("nieznana opcja: %c\n", optopt);
            // Return error code.
            return -5;
            break;
        default:
            // If getopt returned a value other than listed above (it should never happen)
            printf("blad");
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

int directoryValid(const char *path)
{
    // Open the directory.
    DIR *d = opendir(path);
    // If an error occured (e.g. when the directory does not exist)
    if (d == NULL)
        // Return error code.
        return -1;
    // Close the directory. If an error occured
    if (closedir(d) == -1)
        // Return error code.
        return -2;
    // Return the correct ending code meaning that the directory exists and operating on it does not cause errors.
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
        printf("PID procesu potomnego: %i\n", pid);
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
                    syslog(LOG_INFO, "uspienie");
                    // Close the connection to the log.
                    closelog();
                    // Put the daemon to sleep.
                    unsigned int timeLeft = sleep(interval);
                    // Open the connection to the log.
                    openlog("DirSyncD", LOG_ODELAY | LOG_PID, LOG_DAEMON);
                    // In the log, write a message about waking up with sleep time in seconds.
                    syslog(LOG_INFO, "obudzenie; przespano %u s", interval - timeLeft);
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
                syslog(LOG_INFO, "koniec synchronizacji; %i", status);
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
    syslog(LOG_INFO, "zakonczenie; %i", ret);
    // Close the connection to the log.
    closelog();
    // Stop the daemon process.
    exit(ret);
}

int listFiles(DIR *dir, list *files)
{
    struct dirent *entry;
    // Initially, set errno to 0.
    errno = 0;
    // Read a directory entry. If no error occured
    while ((entry = readdir(dir)) != NULL)
    {
        // If the entry is a regular file, add it to the file list. If an error occured
        if (entry->d_type == DT_REG && pushBack(files, entry) < 0)
            // Interrupt returning an error code because directory entry lists must be complete for directory comparison during a synchronization.
            return -1;
    }
    // If an error occured while reading a directory entry, then readdir returned NULL and set errno to a value not equal to 0.
    if (errno != 0)
        // Return an error code.
        return -2;
    // Return the correct ending code.
    return 0;
}
int listFilesAndDirectories(DIR *dir, list *files, list *subdirs)
{
    struct dirent *entry;
    // Initially, set errno to 0.
    errno = 0;
    // Read a directory entry. If no error occured
    while ((entry = readdir(dir)) != NULL)
    {
        // If the entry is a regular file
        if (entry->d_type == DT_REG)
        {
            // Add it to the file list. If an error occured
            if (pushBack(files, entry) < 0)
                 // Interrupt returning an error code because directory entry lists must be complete for directory comparison during a synchronization.
                return -1;
        }
        // If the entry is a directory
        else if (entry->d_type == DT_DIR)
        {
            // If its name is other than '.' and '..'
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0 &&
                // Add it to the directory list. If an error occured
                pushBack(subdirs, entry) < 0)
                // Return an error code.
                return -2;
        }
        // Ignore entries of other types (symbolic links, block devices, character devices, sockets, etc.).
    }
    // if an error occured while reading a directory entry, then readdir returned NULL and set errno to a value not equal to 0.
    if (errno != 0)
        // Return an error code.
        return -3;
    // Return the correct ending code.
    return 0;
}

int createEmptyDirectory(const char *path, mode_t mode)
{
    // Create an empty directory and return a status code.
    return mkdir(path, mode);
}
int removeDirectoryRecursively(const char *path, const size_t pathLength)
{
    // Initially, set status code indicating no error.
    int ret = 0;
    DIR *dir = NULL;
    // Open the source directory. If an error occured
    if ((dir = opendir(path)) == NULL)
        // Set an error code. After that, ther program goes to the end of the current function.
        ret = -1;
    else
    {
        // Create lists for files and subdirectories of the directory.
        list files, subdirs;
        // Initialize the file list.
        initialize(&files);
        // Initialize the subdirectory list.
        initialize(&subdirs);
        // Fill the list. If an error occured
        if (listFilesAndDirectories(dir, &files, &subdirs) < 0)
            // Set an error code.
            ret = -2;
        else
        {
            char *subPath = NULL;
            // Reserve memory for subdirectory and file paths. If an error occured
            if ((subPath = malloc(sizeof(char) * PATH_MAX)) == NULL)
                // Set an error code.
                ret = -3;
            else
            {
                // Copy the directory path as the beginning of its file and subdirectory paths.
                strcpy(subPath, path);
                // Save a pointer to the first subdirectory.
                element *cur = subdirs.first;
                // Recursively remove subdirectories.
                while (cur != NULL)
                {
                    // Append the subdirectory name to its parent directory path. Function removeDirectoryRecursively demands that the directory path end with '/'.
                    size_t subPathLength = appendSubdirectoryName(subPath, pathLength, cur->entry->d_name);
                    // Recursively remove subdirectories. If an error occured
                    if (removeDirectoryRecursively(subPath, subPathLength) < 0)
                        // Set an error code intended for the calling function.
                        ret = -4;
                    // Move the pointer to the next subdirectory.
                    cur = cur->next;
                }
                // Save a pointer to the first file.
                cur = files.first;
                // Remove files.
                while (cur != NULL)
                {
                    // Append the file name to its parent directory path.
                    stringAppend(subPath, pathLength, cur->entry->d_name);
                    // Remove the file. If an error occured
                    if (removeFile(subPath) == -1)
                        // Set an error code.
                        ret = -5;
                    // Move the pointer to the next file.
                    cur = cur->next;
                }
                // Release memory intended for file and subdirectory paths.
                free(subPath);
            }
        }
        // Clear file list.
        clear(&files);
        // Clear subdirectory list.
        clear(&subdirs);
    }
    // A critical error occurs if removing any directory entry is unsuccessful.
    // If the directory was opened, close it. If an error occured and the error code is not negative yet, thus any critical error has not occured yet
    if (dir != NULL && closedir(dir) == -1 && ret >= 0)
        // Set a positive error code indicating a non-critical error.
        ret = 1;
    // If any critical error did not occur, delete the directory. If an error occured
    if (ret >= 0 && rmdir(path) == -1)
        // Set an error code.
        ret = -6;
    // Return the status code.
    return ret;
}

int copySmallFile(const char *srcFilePath, const char *dstFilePath, const mode_t dstMode, const struct timespec *dstAccessTime, const struct timespec *dstModificationTime)
{
    // Initially, set status code indicating no error.
    int ret = 0, in = -1, out = -1;
    // Open the source file for reading and save its descriptor. If an error occured
    if ((in = open(srcFilePath, O_RDONLY)) == -1)
        // Set an error code. After that, the program immediately goes to the end of the current function.
        ret = -1;
    // Open the target file for writing. If it does not exist, create it with empty permissions. Otherwise, clear it. Save its descriptor. If an error occured
    else if ((out = open(dstFilePath, O_WRONLY | O_CREAT | O_TRUNC, 0000)) == -1)
        // Set an error code.
        ret = -2;
    // Set the target file dstMode permissions. If an error occured
    else if (fchmod(out, dstMode) == -1)
        // Set an error code.
        ret = -3;
    else
    {
        // (page 124) Send an advice to the kernel that the source file will be read sequentially so it should be loaded in advance.
        if (posix_fadvise(in, 0, 0, POSIX_FADV_SEQUENTIAL) == -1)
            // Set a non-critical error code because the source file can be read even without the advice but less effectively.
            ret = 1;
        char *buffer = NULL;
        // Optimal buffer size for input/output operations on a file can be checked in its metadata read using stat but we use a predefined size.
        // Reserve buffer memory. If an error occured
        if ((buffer = malloc(sizeof(char) * BUFFERSIZE)) == NULL)
            // Set an error code.
            ret = -4;
        else
        {
            while (1)
            {
                // The algorithm below is on page 45.
                // Position in the buffer.
                char *position = buffer;
                // Save the total number of bytes remaining to be read.
                size_t remainingBytes = BUFFERSIZE;
                ssize_t bytesRead;
                // While numbers of remaining bytes and bytes read in the current iteration are non-zero.
                while (remainingBytes != 0 && (bytesRead = read(in, position, remainingBytes)) != 0)
                {
                    // If an error occured in function read.
                    if (bytesRead == -1)
                    {
                        // If function read was interrupted by receiving a signal. SIGUSR1 and SIGTERM are blocked for the synchronization so those signals cannot cause this error.
                        if (errno == EINTR)
                            // Retry reading.
                            continue;
                        // If other error occured
                        // Set an error code.
                        ret = -5;
                        // BUFFERSIZE - BUFFERSIZE == 0 so the second loop is not executed
                        remainingBytes = BUFFERSIZE;
                        // Set 0 so condition if (bytesRead == 0) breaks external loop while (1).
                        bytesRead = 0;
                        // Break the inner loop.
                        break;
                    }
                    // Decrease the number of remaining bytes by the number of bytes read in the current iteration and
                    remainingBytes -= bytesRead;
                    // move the position in the buffer.
                    position += bytesRead;
                }
                position = buffer; // page 48
                remainingBytes = BUFFERSIZE - remainingBytes; // Save the total number of read bytes that is always less or equal to the buffer size.
                ssize_t bytesWritten;
                // While numbers of remaining bytes and bytes written in the current iteration are non-zero.
                while (remainingBytes != 0 && (bytesWritten = write(out, position, remainingBytes)) != 0)
                {
                    // If an error occured in function write.
                    if (bytesWritten == -1)
                    {
                        // If function write was interrupted by receiving a signal.
                        if (errno == EINTR)
                            // Retry writing.
                            continue;
                        // If other error occured
                        // Set an error code.
                        ret = -6;
                        // Set 0 so condition if (bytesRead == 0) breaks external loop while (1).
                        bytesRead = 0;
                        // Break the inner loop.
                        break;
                    }
                    // Decrease the number of remaining bytes by the number of bytes written in the current iteration and
                    remainingBytes -= bytesWritten;
                    // move the position in the buffer.
                    position += bytesWritten;
                }
                // If we came to the end of the source file (EOF) or an error occured
                if (bytesRead == 0)
                    // Break external loop while (1).
                    break;
            }
            // Free buffer memory.
            free(buffer);
            // If no error occured while copying
            if (ret >= 0)
            {
                // Create a structure containing last access and modification times.
                const struct timespec times[2] = {*dstAccessTime, *dstModificationTime};
                // Must be set after writing the target file ends because writing sets the modification time to the current operating system time. Set the times of the target file. If an error occured
                if (futimens(out, times) == -1)
                    // Set an error code.
                    ret = -7;
            }
        }
    }
    // If the source file was opened, close it. If an error occured
    if (in != -1 && close(in) == -1)
        // Set an error code. Do not consider it a not-critical error because every unclosed file occupies a descriptor of which the process can have only a finite number.
        ret = -8;
    // If the target file was opened, close it. If an error occured
    if (out != -1 && close(out) == -1)
        // Set an error code.
        ret = -9;
    // Return the status code.
    return ret;
}
int copyBigFile(const char *srcFilePath, const char *dstFilePath, const unsigned long long fileSize, const mode_t dstMode, const struct timespec *dstAccessTime, const struct timespec *dstModificationTime)
{
    // Initially, set status code indicating no error.
    int ret = 0, in = -1, out = -1;
    // Open the source file for reading and save its descriptor. If an error occured
    if ((in = open(srcFilePath, O_RDONLY)) == -1)
        // Set an error code. After that, the program immediately goes to the end of the current function.
        ret = -1;
    // Open the target file for writing. If it does not exist, create it with empty permissions. Otherwise, clear it. Save its descriptor. If an error occured
    else if ((out = open(dstFilePath, O_WRONLY | O_CREAT | O_TRUNC, 0000)) == -1)
        // Set an error code.
        ret = -2;
    // Set the target file dstMode permissions. If an error occured
    else if (fchmod(out, dstMode) == -1)
        // Set an error code.
        ret = -3;
    else
    {
        char *map;
        // Map the source file in memory for reading. If an error occured
        if ((map = mmap(0, fileSize, PROT_READ, MAP_SHARED, in, 0)) == MAP_FAILED)
            // Set an error code.
            ret = -4;
        else
        {
            // (page 121) Send an advice to the kernel that the source file will be read sequentially so it should be loaded in advance. If an error occured
            if (madvise(map, fileSize, MADV_SEQUENTIAL) == -1)
                // Set a non-critical error code because the source file can be read even without the advice but less effectively.
                ret = 1;
            char *buffer = NULL;
            // Optimal buffer size for input/output operations on a file can be checked in its metadata read using stat but we use a predefined size.
            // Reserve buffer memory. If an error occured
            if ((buffer = malloc(sizeof(char) * BUFFERSIZE)) == NULL)
                // Set an error code.
                ret = -5;
            else
            {
                // Byte index in the source file.
                unsigned long long b;
                // Position in the buffer.
                char *position;
                size_t remainingBytes;
                ssize_t bytesWritten;
                // Cannot be (b < fileSize - BUFFERSIZE) because b and fileSize are of unsigned type so if fileSize < BUFFERSIZE and we subtract, then we have an overflow.
                for (b = 0; b + BUFFERSIZE < fileSize; b += BUFFERSIZE)
                {
                    // Copy BUFFERSIZE (size of the buffer) bytes from the mapped memory to the buffer.
                    memcpy(buffer, map + b, BUFFERSIZE);
                    // The algorithm below is on page 48.
                    position = buffer;
                    // Save the total number of bytes remaining to be written which is always equal to buffer size.
                    remainingBytes = BUFFERSIZE;
                    // While numbers of remaining bytes and bytes written in the current iteration are non-zero.
                    while (remainingBytes != 0 && (bytesWritten = write(out, position, remainingBytes)) != 0)
                    {
                        // If an error occured in function write.
                        if (bytesWritten == -1)
                        {
                            // If function write was interrupted by receiving a signal. SIGUSR1 and SIGTERM are blocked for the synchronization so those signals cannot cause this error.
                            if (errno == EINTR)
                                // Retry writing.
                                continue;
                            // If other error occured
                            // Set an error code.
                            ret = -6;
                            // Set b to break external loop for.
                            b = ULLONG_MAX - BUFFERSIZE;
                            // Break the inner loop.
                            break;
                        }
                        // Decrease the number of remaining bytes by the number of bytes written in the current iteration and
                        remainingBytes -= bytesWritten;
                        // move the position in the buffer.
                        position += bytesWritten;
                    }
                }
                // If no error occured while copying
                if (ret >= 0)
                {
                    // Save the number of bytes located at the end of the source file which did not fit into one full buffer.
                    remainingBytes = fileSize - b;
                    // Copy them from the mapped memory to the buffer.
                    memcpy(buffer, map + b, remainingBytes);
                    // Save the position of the first byte in the buffer.
                    position = buffer;
                    // While numbers of remaining bytes and bytes written in the current iteration are non-zero.
                    while (remainingBytes != 0 && (bytesWritten = write(out, position, remainingBytes)) != 0)
                    {
                        // If an error occured in function write.
                        if (bytesWritten == -1)
                        {
                            // If function write was interrupted by receiving a signal.
                            if (errno == EINTR)
                                // Retry writing.
                                continue;
                            // Set an error code.
                            ret = -7;
                            // Break the inner loop.
                            break;
                        }
                        // Decrease the number of remaining bytes by the number of bytes written in the current iteration and
                        remainingBytes -= bytesWritten;
                        // move the position in the buffer.
                        position += bytesWritten;
                    }
                }
                // Free buffer memory.
                free(buffer);
                // If no error occured while copying
                if (ret >= 0)
                {
                    // Create a structure containing last access and modification times.
                    const struct timespec times[2] = {*dstAccessTime, *dstModificationTime};
                    // Must be set after writing the target file ends because writing sets the modification time to the current operating system time. Set the times of the target file. If an error occured
                    if (futimens(out, times) == -1)
                        // Set an error code.
                        ret = -8;
                }
            }
            // Unmap the source file in memory. If an error occured
            if (munmap(map, fileSize) == -1)
                // Set an error code.
                ret = -9;
        }
    }
    // If the source file was opened, close it. If an error occured
    if (in != -1 && close(in) == -1)
        // Set an error code. Do not consider it a not-critical error because every unclosed file occupies a descriptor of which the process can have only a finite number.
        ret = -10;
    // If the target file was opened, close it. If an error occured
    if (out != -1 && close(out) == -1)
        // Set an error code.
        ret = -11;
    // Return the status code.
    return ret;
}
int removeFile(const char *path)
{
    // Unlink is used only for removing files. A directory we remove using rmdir but first we have to empty it. Remove the file and return a status code.
    return unlink(path);
}

int updateDestinationFiles(const char *srcDirPath, const size_t srcDirPathLength, list *filesSrc, const char *dstDirPath, const size_t dstDirPathLength, list *filesDst)
{
    char *srcFilePath = NULL, *dstFilePath = NULL;
    // Reserve memory for paths of files in the source directory. If an error occured
    if ((srcFilePath = malloc(sizeof(char) * PATH_MAX)) == NULL)
        // Return an error code.
        return -1;
    // Reserve memory for paths of files in the target directory. If an error occured
    if ((dstFilePath = malloc(sizeof(char) * PATH_MAX)) == NULL)
    {
        // Release already reserved memory.
        free(srcFilePath);
        // Return an error code.
        return -2;
    }
    // Copy the source directory path as the beginning of its file paths.
    strcpy(srcFilePath, srcDirPath);
    // Copy the target directory path as the beginning of its file paths.
    strcpy(dstFilePath, dstDirPath);
    // Save pointers to the first source and target files.
    element *curS = filesSrc->first, *curD = filesDst->first;
    struct stat srcFile, dstFile;
    // Initially, set status code indicating no error.
    int status = 0, ret = 0;
    // Open a connection to the log '/var/log/syslog'.
    openlog("DirSyncD", LOG_ODELAY | LOG_PID, LOG_DAEMON);
    while (curS != NULL && curD != NULL)
    {
        char *srcFileName = curS->entry->d_name, *dstFileName = curD->entry->d_name;
        // Compare source and target file names in lexicographic order.
        int comparison = strcmp(srcFileName, dstFileName);
        // If the source file is greater than the target file in the order
        if (comparison > 0)
        {
            // Append target file name to its parent directory path.
            stringAppend(dstFilePath, dstDirPathLength, dstFileName);
            // Remove the target file.
            status = removeFile(dstFilePath);
            // In the log, write a message about removal.
            syslog(LOG_INFO, "usuwamy plik %s; %i\n", dstFilePath, status);
            // If an error occured
            if (status != 0)
                // Set a positive error code to indicate partial synchronization but do not break the loop.
                ret = 1;
            // Move the pointer to the next target file.
            curD = curD->next;
        }
        else
        {
            // Append source file name to its parent directory path.
            stringAppend(srcFilePath, srcDirPathLength, srcFileName);
            // Read source file metadata. If an error occured, the source file is unavailable and will not be able to be copied when comparison < 0.
            if (stat(srcFilePath, &srcFile) == -1)
            {
                // If the source file is less than the target file in the order
                if (comparison < 0)
                {
                    // In the log, write a message about unsuccessful copying. The status code written to errno by stat is a positive number.
                    syslog(LOG_INFO, "kopiujemy plik %s do katalogu %s; %i\n", srcFilePath, dstDirPath, errno);
                    // Set an error code.
                    ret = 2;
                }
                // If the source file is equal to the target file in the order
                else
                {
                    // In the log, save a message about unsuccessful metadata reading.
                    syslog(LOG_INFO, "odczytujemy metadane pliku źródłowego %s; %i\n", srcFilePath, errno);
                    // Move the pointer to the next target file.
                    curD = curD->next;
                    // Set an error code.
                    ret = 3;
                }
                // Move the pointer to the next source file.
                curS = curS->next;
                // Go to the next loop iteration.
                continue;
            }
            // If metadata was read correctly
            // If the source file is less than the target file in the order
            if (comparison < 0)
            {
                // Append source file name to the target directory path.
                stringAppend(dstFilePath, dstDirPathLength, srcFileName);
                // If the source file is smaller than the big file threshold
                if (srcFile.st_size < threshold)
                    // Copy it as a small file. Copy permissions and modification time of the source file to the target file.
                    status = copySmallFile(srcFilePath, dstFilePath, srcFile.st_mode, &srcFile.st_atim, &srcFile.st_mtim);
                // If the source file is bigger or the same size as the big file threshold
                else
                    // Copy it as a big file.
                    status = copyBigFile(srcFilePath, dstFilePath, srcFile.st_size, srcFile.st_mode, &srcFile.st_atim, &srcFile.st_mtim);
                // In the log, write a message about copying.
                syslog(LOG_INFO, "kopiujemy plik %s do katalogu %s; %i\n", srcFilePath, dstDirPath, status);
                // If an error occured
                if (status != 0)
                    // Set an error code.
                    ret = 4;
                // Move the pointer to the next source file.
                curS = curS->next;
            }
            // If the source file is equal to the target file in the order
            else
            {
                // Append target file name to its parent directory path.
                stringAppend(dstFilePath, dstDirPathLength, dstFileName);
                // Read target file metadata. If an error occured, the target file is unavailable and we will not be able to compare modification times.
                if (stat(dstFilePath, &dstFile) == -1)
                {
                    // In the log, save a message about unsuccessful metadata reading.
                    syslog(LOG_INFO, "odczytujemy metadane pliku docelowego %s; %i\n", dstFilePath, errno);
                    // Set an error code.
                    ret = 5;
                }
                // If metadata was read correctly and the target file has other modification time than the source file (earlier - target is outdated or later - target was modified)
                else if (srcFile.st_mtim.tv_sec != dstFile.st_mtim.tv_sec || srcFile.st_mtim.tv_nsec != dstFile.st_mtim.tv_nsec)
                {
                    // Copy the source file to an existing target file.
                    // If the source file is smaller than the big file threshold
                    if (srcFile.st_size < threshold)
                        // Copy it as a small file.
                        status = copySmallFile(srcFilePath, dstFilePath, srcFile.st_mode, &srcFile.st_atim, &srcFile.st_mtim);
                    // If the source file is bigger or the same size as the big file threshold
                    else
                        // Copy it as a big file.
                        status = copyBigFile(srcFilePath, dstFilePath, srcFile.st_size, srcFile.st_mode, &srcFile.st_atim, &srcFile.st_mtim);
                    // In the log, write a message about copying.
                    syslog(LOG_INFO, "przepisujemy %s do %s; %i\n", srcFilePath, dstFilePath, status);
                    // If an error occured
                    if (status != 0)
                        // Set an error code.
                        ret = 6;
                }
                // After copying, copy permissions but if the file was not copied, check if both files have equal permissions. If the files have different permissions
                else if (srcFile.st_mode != dstFile.st_mode)
                {
                    // Copy permissions from the source file to the target file. If an error occured
                    if (chmod(dstFilePath, srcFile.st_mode) == -1)
                    {
                        // Set status code not equal to 0 because errno has value not equal to 0.
                        status = errno;
                        // Set an error code.
                        ret = 7;
                    }
                    // If no error occured
                    else
                        // Set status code equal to 0.
                        status = 0;
                    // In the log, write a message about copying permissions.
                    syslog(LOG_INFO, "przepisujemy uprawnienia pliku %s do %s; %i\n", srcFilePath, dstFilePath, status);
                }
                // Move the pointer to the next source file.
                curS = curS->next;
                // Move the pointer to the next target file.
                curD = curD->next;
            }
        }
    }
    // If any remaining files exist in the target directory, remove them because they do not exist in the source directory. Start removing at the file currently pointed to by curD.
    while (curD != NULL)
    {
        char *dstFileName = curD->entry->d_name;
        // Append target file name to its parent directory path.
        stringAppend(dstFilePath, dstDirPathLength, dstFileName);
        // Remove the target file.
        status = removeFile(dstFilePath);
        // In the log, write a message about removal.
        syslog(LOG_INFO, "usuwamy plik %s; %i\n", dstFilePath, status);
        // If an error occured
        if (status != 0)
            // Set an error code.
            ret = 8;
        // Move the pointer to the next target file.
        curD = curD->next;
    }
    // If any remaining files exist in the source directory, copy them because they do not exist in the target directory. Start copying at the file currently pointed to by curS.
    while (curS != NULL)
    {
        char *srcFileName = curS->entry->d_name;
        // Append source file name to its parent directory path.
        stringAppend(srcFilePath, srcDirPathLength, srcFileName);
        // Read source file metadata. If an error occured
        if (stat(srcFilePath, &srcFile) == -1)
        {
            // In the log, write a message about unsuccessful copying.
            syslog(LOG_INFO, "kopiujemy plik %s do katalogu %s; %i\n", srcFilePath, dstDirPath, errno);
            // Set an error code.
            ret = 9;
        }
        else
        {
            // Append source file name to the target directory name.
            stringAppend(dstFilePath, dstDirPathLength, srcFileName);
            // If the source file is smaller than the big file threshold
            if (srcFile.st_size < threshold)
                // Copy it as a small file.
                status = copySmallFile(srcFilePath, dstFilePath, srcFile.st_mode, &srcFile.st_atim, &srcFile.st_mtim);
            // If the source file is bigger or the same size as the big file threshold
            else
                // Copy it as a big file.
                status = copyBigFile(srcFilePath, dstFilePath, srcFile.st_size, srcFile.st_mode, &srcFile.st_atim, &srcFile.st_mtim);
            // In the log, write a message about copying.
            syslog(LOG_INFO, "kopiujemy plik %s do katalogu %s; %i\n", srcFilePath, dstDirPath, status);
            // If an error occured
            if (status != 0)
                // Set an error code.
                ret = 10;
        }
        // Move the pointer to the next source file.
        curS = curS->next;
    }
    // Release source file path memory.
    free(srcFilePath);
    // Release target file path memory.
    free(dstFilePath);
    // Close the connection to the log.
    closelog();
    // Return the status code.
    return ret;
}
int updateDestinationDirectories(const char *srcDirPath, const size_t srcDirPathLength, list *subdirsSrc, const char *dstDirPath, const size_t dstDirPathLength, list *subdirsDst, char *isReady)
{
    char *srcSubdirPath = NULL, *dstSubdirPath = NULL;
    // Reserve memory for paths of subdirectories in the source directory (hereinafter referred to as source subdirectories). If an error occured
    if ((srcSubdirPath = malloc(sizeof(char) * PATH_MAX)) == NULL)
        // Return an error code.
        return -1;
    // Reserve memory for paths of subdirectories in the target directory (hereinafter referred to as target subdirectories). If an error occured
    if ((dstSubdirPath = malloc(sizeof(char) * PATH_MAX)) == NULL)
    {
        // Release already reserved memory.
        free(srcSubdirPath);
        // Return an error code.
        return -2;
    }
    // Copy the source directory path as the beginning of its subdirectory paths.
    strcpy(srcSubdirPath, srcDirPath);
    // Copy the target directory path as the beginning of its subdirectory paths.
    strcpy(dstSubdirPath, dstDirPath);
    // Save pointers to the first source and target subdirectories.
    element *curS = subdirsSrc->first, *curD = subdirsDst->first;
    struct stat srcSubdir, dstSubdir;
    unsigned int i = 0;
    // Initially, set status code indicating no error.
    int status = 0, ret = 0;
    // Open a connection to the log '/var/log/syslog'.
    openlog("DirSyncD", LOG_ODELAY | LOG_PID, LOG_DAEMON);
    while (curS != NULL && curD != NULL)
    {
        char *srcSubdirName = curS->entry->d_name, *dstSubdirName = curD->entry->d_name;
        // Compare source and target subdirectory names in lexicographic order.
        int comparison = strcmp(srcSubdirName, dstSubdirName);
        // If the source subdirectory is greater than the target subdirectory in the order
        if (comparison > 0)
        {
            // Append target subdirectory name to its parent directory path. Function removeDirectoryRecursively demands that the directory path end with '/'.
            size_t length = appendSubdirectoryName(dstSubdirPath, dstDirPathLength, dstSubdirName);
            // Recursively remove the target subdirectory.
            status = removeDirectoryRecursively(dstSubdirPath, length);
            // In the log, write a message about removal.
            syslog(LOG_INFO, "usuwamy katalog %s; %i\n", dstSubdirPath, status);
            // If an error occured
            if (status != 0)
                // Set a positive error code to indicate partial synchronization but do not break the loop.
                ret = 1;
            // Move the pointer to the next target subdirectory.
            curD = curD->next;
        }
        else
        {
            // Append source subdirectory name to its parent directory path.
            stringAppend(srcSubdirPath, srcDirPathLength, srcSubdirName);
            // Read source subdirectory metadata. If an error occured, the source subdirectory is unavailable and will not be able to be created when comparison < 0. Even if we created it, we would not be able to synchronize it.
            if (stat(srcSubdirPath, &srcSubdir) == -1)
            {
                // If the source subdirectory is less than the target subdirectory in the order
                if (comparison < 0)
                {
                    // In the log, write a message about unsuccessful copying. The status code written to errno by stat is a positive number.
                    syslog(LOG_INFO, "tworzymy katalog %s; %i\n", dstSubdirPath, errno);
                    // Indicate that the subdirectory is unready for synchronization because it does not exist.
                    isReady[i++] = 0;
                    // Set an error code.
                    ret = 2;
                }
                else
                {
                    // In the log, save a message about unsuccessful metadata reading.
                    syslog(LOG_INFO, "odczytujemy metadane katalogu źródłowego %s; %i\n", srcSubdirPath, errno);
                    // If we did not manage to check if source and target subdirectories have equal permissions, assume that they do. Indicate that the subdirectory is ready for synchronization.
                    isReady[i++] = 1;
                    // Set an error code.
                    ret = 3;
                    // Move the pointer to the next target subdirectory.
                    curD = curD->next;
                }
                // Move the pointer to the next source subdirectory.
                curS = curS->next;
                // Go to the next loop iteration.
                continue;
            }
            // If metadata was read correctly
            // If the source subdirectory is less than the target subdirectory in the order
            if (comparison < 0)
            {
                // Append source subdirectory name to the target directory path.
                stringAppend(dstSubdirPath, dstDirPathLength, srcSubdirName);
                // In the target directory, create a subdirectory named the same as the source subdirectory. Copy permissions but do not copy modification time because we ignore it during synchronization - all subdirectories are browsed to detect file changes.
                status = createEmptyDirectory(dstSubdirPath, srcSubdir.st_mode);
                // In the log, write a meesage about creation.
                syslog(LOG_INFO, "tworzymy katalog %s; %i\n", dstSubdirPath, status);
                // If an error occured
                if (status != 0)
                {
                    // Indicate that the subdirectory is unready for synchronization because it does not exist.
                    isReady[i++] = 0;
                    // Set an error code.
                    ret = 4;
                }
                else
                    // Indicate that the subdirectory is ready for synchronization.
                    isReady[i++] = 1;
                // Move the pointer to the next source subdirectory.
                curS = curS->next;
            }
            // If the source subdirectory is equal to the target subdirectory in the order
            else
            {
                // Indicate that the subdirectory is ready for synchronization even if permission comparison is unsuccessful.
                isReady[i++] = 1;
                // Append target subdirectory name to its parent directory path.
                stringAppend(dstSubdirPath, dstDirPathLength, dstSubdirName);
                // Read target subdirectory metadata. If an error occured, the target subdirectory is unavailable and we will not be able to compare permissions.
                if (stat(dstSubdirPath, &dstSubdir) == -1)
                {
                    // In the log, save a message about unsuccessful metadata reading.
                    syslog(LOG_INFO, "odczytujemy metadane katalogu docelowego %s; %i\n", dstSubdirPath, errno);
                    // Set an error code.
                    ret = 5;
                }
                // Ignore subdirectory modification time (it changes on file creation and deletion in the subdirectory).
                // If metadata was read correctly and source and target directories have different permissions
                else if (srcSubdir.st_mode != dstSubdir.st_mode)
                {
                    // Copy permissions from source to target subdirectory. If an error occured
                    if (chmod(dstSubdirPath, srcSubdir.st_mode) == -1)
                    {
                        // Set status code not equal to 0 because errno has value not equal to 0.
                        status = errno;
                        // Set an error code.
                        ret = 6;
                    }
                    // If no error occured
                    else
                        // Set status code equal to 0.
                        status = 0;
                    // In the log, write a message about copying permissions.
                    syslog(LOG_INFO, "przepisujemy uprawnienia katalogu %s do %s; %i\n", srcSubdirPath, dstSubdirPath, status);
                }
                // Move the pointer to the next source subdirectory.
                curS = curS->next;
                // Move the pointer to the next target subdirectory.
                curD = curD->next;
            }
        }
    }
    // If any remaining subdirectories exist in the target directory, remove them because they do not exist in the source directory. Start removing at the subdirectory currently pointed to by curD.
    while (curD != NULL)
    {
        char *dstSubdirName = curD->entry->d_name;
        // Append target subdirectory name to its parent directory path.
        size_t length = appendSubdirectoryName(dstSubdirPath, dstDirPathLength, dstSubdirName);
        // Recursively remove the target subdirectory.
        status = removeDirectoryRecursively(dstSubdirPath, length);
        // In the log, write a message about removal.
        syslog(LOG_INFO, "usuwamy katalog %s; %i\n", dstSubdirPath, status);
        // If an error occured
        if (status != 0)
            // Set an error code.
            ret = 7;
        // Move the pointer to the next target subdirectory.
        curD = curD->next;
    }
    // If any remaining subdirectories exist in the source directory, copy them because they do not exist in the target directory. Start copying at the file currently pointed to by curS.
    while (curS != NULL)
    {
        char *srcSubdirName = curS->entry->d_name;
        // Append source subdirectory name to its parent directory path.
        stringAppend(srcSubdirPath, srcDirPathLength, srcSubdirName);
        // Read source subdirectory metadata. If an error occured
        if (stat(srcSubdirPath, &srcSubdir) == -1)
        {
            // In the log, write a message about unsuccessful creation.
            syslog(LOG_INFO, "tworzymy katalog %s; %i\n", dstSubdirPath, errno);
            // Indicate that the subdirectory is unready for synchronization because it does not exist.
            isReady[i++] = 0;
            // Set an error code.
            ret = 8;
            // Move the pointer to the next source subdirectory.
            curS = curS->next;
            // Go to the next loop iteration.
            continue;
        }
        // Append source subdirectory name to the target directory name.
        stringAppend(dstSubdirPath, dstDirPathLength, srcSubdirName);
        // In the target directory, create a subdirectory named the same as the source subdirectory and copy permissions.
        status = createEmptyDirectory(dstSubdirPath, srcSubdir.st_mode);
        // In the log, write a message about creation.
        syslog(LOG_INFO, "tworzymy katalog %s; %i\n", dstSubdirPath, status);
        // If an error occured
        if (status != 0)
        {
            // Indicate that the subdirectory is unready for synchronization because it does not exist.
            isReady[i++] = 0;
            // Set an error code.
            ret = 9;
        }
        else
            // Indicate that the subdirectory is ready for synchronization.
            isReady[i++] = 1;
        // Move the pointer to the next source subdirectory.
        curS = curS->next;
    }
    // Release source subdirectory path memory.
    free(srcSubdirPath);
    // Release target subdirectory path memory.
    free(dstSubdirPath);
    // Close the connection to the log.
    closelog();
    // Return the status code.
    return ret;
}

int synchronizeNonRecursively(const char *sourcePath, const size_t sourcePathLength, const char *destinationPath, const size_t destinationPathLength)
{
    // Initially, set status code indicating no error.
    int ret = 0;
    DIR *dirS = NULL, *dirD = NULL;
    // Open the source directory. If an error occured
    if ((dirS = opendir(sourcePath)) == NULL)
        // Set status code indicating an error. After that, the program immediately goes to the end of the current function.
        ret = -1;
    // Open the target directory. If an error occured
    else if ((dirD = opendir(destinationPath)) == NULL)
        // Set status code indicating an error.
        ret = -2;
    else
    {
        // Create lists for source and target directory files.
        list filesS, filesD;
        // Initialize the source directory file list.
        initialize(&filesS);
        // Initialize the target directory file list.
        initialize(&filesD);
        // Fill the source directory file list. If an error occured
        if (listFiles(dirS, &filesS) < 0)
            // Set status code indicating an error.
            ret = -3;
        // Fill the target directory file list. If an error occured
        else if (listFiles(dirD, &filesD) < 0)
            // Set status code indicating an error.
            ret = -4;
        else
        {
            // Sort the source directory file list.
            listMergeSort(&filesS);
            // Sort the target directory file list.
            listMergeSort(&filesD);
            // Check compliance and if needed, update target directory files. If an error occured
            if (updateDestinationFiles(sourcePath, sourcePathLength, &filesS, destinationPath, destinationPathLength, &filesD) != 0)
                // Set status code indicating an error.
                ret = -5;
        }
        // Clear the source directory file list.
        clear(&filesS);
        // Clear the target directory file list.
        clear(&filesD);
    }
    // If an error occured somewhere, go here.
    // Objects of type dirent, which we read from the directory using readdir, are removed from memory on calling closedir. Therefore, we cannot close the directory until we finish using dirent objects. If the source directory was opened
    if (dirS != NULL)
        // Close the source directory. If an error occured, ignore it.
        closedir(dirS);
    // If the target directory was opened
    if (dirD != NULL)
        // Close the target directory. If an error occured, ignore it.
        closedir(dirD);
    // Return the status code.
    return ret;
}
int synchronizeRecursively(const char *sourcePath, const size_t sourcePathLength, const char *destinationPath, const size_t destinationPathLength)
{
    // Initially, set status code indicating no error.
    int ret = 0;
    DIR *dirS = NULL, *dirD = NULL;
    // Open the source directory. If an error occured
    if ((dirS = opendir(sourcePath)) == NULL)
        // Set status code indicating an error. After that, the program immediately goes to the end of the current function.
        ret = -1;
    // Open the target directory. If an error occured
    else if ((dirD = opendir(destinationPath)) == NULL)
        // Set status code indicating an error.
        ret = -2;
    else
    {
        // Create lists for source directory files and subdirectories.
        list filesS, subdirsS;
        // Initialize the source directory file list.
        initialize(&filesS);
        // Initialize the source directory subdirectory list.
        initialize(&subdirsS);
        // Create lists for target directory files and subdirectories.
        list filesD, subdirsD;
        // Initialize the target directory file list.
        initialize(&filesD);
        // Initialize the target directory subdirectory list.
        initialize(&subdirsD);
        // Fill the source directory file and subdirectory lists. If an error occured
        if (listFilesAndDirectories(dirS, &filesS, &subdirsS) < 0)
            // Set status code indicating an error.
            ret = -3;
        // Fill the target directory file and subdirectory lists. If an error occured
        else if (listFilesAndDirectories(dirD, &filesD, &subdirsD) < 0)
            // Set status code indicating an error.
            ret = -4;
        else
        {
            // Sort the source directory file list.
            listMergeSort(&filesS);
            // Sort the target directory file list.
            listMergeSort(&filesD);
            // Check compliance and if needed, update target directory files. If an error occured
            if (updateDestinationFiles(sourcePath, sourcePathLength, &filesS, destinationPath, destinationPathLength, &filesD) != 0)
                // Set status code indicating an error.
                ret = -5;
            // Clear the source directory file list.
            clear(&filesS);
            // Clear the target directory file list.
            clear(&filesD);

            // Sort the source directory subdirectory list.
            listMergeSort(&subdirsS);
            // Sort the target directory subdirectory list.
            listMergeSort(&subdirsD);
            // Set i-th cell of array isReady to 1 if i-th source subdirectory exists or will be correctly created in the target directory by function updateDestinationDirectories so it will be ready for recursive synchronization.
            char *isReady = NULL;
            // Reserve memory for an array with size equal to the number of source subdirectories. If an error occured
            if ((isReady = malloc(sizeof(char) * subdirsS.count)) == NULL)
                // Set status code indicating an error.
                ret = -6;
            else
            {
                // Check compliance and if needed, update target directory subdirectories. Fill array isReady. If an error occured
                if (updateDestinationDirectories(sourcePath, sourcePathLength, &subdirsS, destinationPath, destinationPathLength, &subdirsD, isReady) != 0)
                    // Set status code indicating an error.
                    ret = -7;
                // Do not clear source subdirectory list yet because function synchronizeRecursively will be recursively called on subdirectories from that list.
                // Clear target subdirectory list.
                clear(&subdirsD);

                char *nextSourcePath = NULL, *nextDestinationPath = NULL;
                // Reserve memory for source subdirectory paths. If an error occured
                if ((nextSourcePath = malloc(sizeof(char) * PATH_MAX)) == NULL)
                    // Set status code indicating an error.
                    ret = -8;
                // Reserve memory for target subdirectory paths. If an error occured
                else if ((nextDestinationPath = malloc(sizeof(char) * PATH_MAX)) == NULL)
                    // Set status code indicating an error.
                    ret = -9;
                else
                {
                    // Copy the source directory path as the beginning of its subdirectory paths.
                    strcpy(nextSourcePath, sourcePath);
                    // Copy the target directory path as the beginning of its subdirectory paths.
                    strcpy(nextDestinationPath, destinationPath);
                    // Save a pointer to the first source subdirectory.
                    element *curS = subdirsS.first;
                    unsigned int i = 0;
                    while (curS != NULL)
                    {
                        // If the subdirectory is ready for synchronization
                        if (isReady[i++] == 1)
                        {
                            // Create the source subdirectory path and save its length.
                            size_t nextSourcePathLength = appendSubdirectoryName(nextSourcePath, sourcePathLength, curS->entry->d_name);
                            // Create the target subdirectory path and save its length.
                            size_t nextDestinationPathLength = appendSubdirectoryName(nextDestinationPath, destinationPathLength, curS->entry->d_name);
                            // Recursively synchronize subdirectories. If an error occured
                            if (synchronizeRecursively(nextSourcePath, nextSourcePathLength, nextDestinationPath, nextDestinationPathLength) < 0)
                                // Set status code indicating an error.
                                ret = -10;
                        }
                        // If the subdirectory is unready for synchronization, skip it.
                        // Move the pointer to the next subdirectory.
                        curS = curS->next;
                    }
                }
                // Free array isReady.
                free(isReady);
                // If memory for source subdirectory paths was reserved
                if (nextSourcePath != NULL)
                    // Release memory.
                    free(nextSourcePath);
                // If memory for target directory paths was reserved
                if (nextDestinationPath != NULL)
                    // Release memory.
                    free(nextDestinationPath);
            }
        }
        // Clear the source subdirectory list.
        clear(&subdirsS);
        // If reserving memory for isReady failed, the list subdirsD of target subdirectories was not cleared. If it contains any nodes
        if (subdirsD.count != 0)
            // Clear it.
            clear(&subdirsD);
    }
    
    // If the source directory was opened
    if (dirS != NULL)
        // Close the source directory. If an error occured, ignore it.
        closedir(dirS);
    // If the target directory was opened
    if (dirD != NULL)
        // Close the target directory. If an error occured, ignore it.
        closedir(dirD);
    // Return the status code.
    return ret;
}
