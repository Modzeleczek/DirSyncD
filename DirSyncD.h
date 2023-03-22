#ifndef DIRSYNCD_H
#define DIRSYNCD_H

#include <dirent.h>
#include <sys/stat.h>

typedef struct element element;
/*
Singly linked list node storing pointers to a directory entry and to next list node.
*/
struct element
{
    // Next list node.
    element *next;
    // Pointer to a directory entry (file or subdirectory).
    struct dirent *entry;
};
/*
Compares nodes of a singly linked list.
reads:
a - first node
b - second node
returns:
< 0 if a is before b in lexicographic order by directory entry name
0 if a and b have equal directory entry names
> 0 if a is after b in lexicographic order by directory entry name
*/
int cmp(element *a, element *b);

typedef struct list list;
/*
Singly linked list. In functions operating on a list, we assume that a valid pointer to it is given.
*/
struct list
{
    // The first and last nodes. Save a pointer to the last node to add new nodes to the list in constant time.
    element *first, *last;
    // Number of list nodes.
    unsigned int count;
};
/*
Initializes the singly linked list.
writes:
l - empty singly linked list intended for the first use
*/
void initialize(list *l);
/*
Adds a directory entry at the end of the list.
reads:
newEntry - directory entry to be added at the end of the list
writes:
l - singly linked list with added node containing directory entry newEntry
*/
int pushBack(list *l, struct dirent *newEntry);
/*
Clears the singly linked list.
writes:
l - empty singly linked list intended for reuse
*/
void clear(list *l);
/* https://www.chiark.greenend.org.uk/~sgtatham/algorithms/listsort.html
 * This file is copyright 2001 Simon Tatham.
 * 
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL SIMON TATHAM BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
/*
Sorts by merging the singly linked list. This function contains author's original comments.
writes:
l - singly linked list sorted using cmp function comparing nodes
*/
void listMergeSort(list *l);

/*
Appends string src to string dst, starting at position with index offset.
reads:
offset - byte index string dst at which we start inserting string src
src - string inserted to string dst
writes:
dst - string into which we insert string src
*/
void stringAppend(char *dst, const size_t offset, const char *src);
/*
Appends the name of a subdirectory to its parent directory path.
reads:
path - path of the directory containing the subdirectory named subName
pathLength - length in bytes of path
subName - name of the subdirectory
writes:
path - path of the subdirectory
returns:
length in bytes of the subdirectory path with character '/' at its end
*/
size_t appendSubdirectoryName(char *path, const size_t pathLength, const char *subName);

/*
Analyzes options and arguments passed to the program and in its parameters writes values ready for use.
reads:
argc - number of program parameters (options and arguments together)
argv - programu parameters
writes:
source - source directory path
destination - target directory path
interval - sleep time in seconds
recursive - recursive directory synchronization (boolean)
threshold - minimal file size to consider it big (this function stores threshold in a global variable)
returns:
< 0 if an error occured
0 if no error occured
*/
int parseParameters(int argc, char **argv, char **source, char **destination, unsigned int *interval, char *recursive);

/*
Checks if a directory exists and is ready for use.
reads:
path - directory path
returns:
-1 if an error occured while opening the directory
-2 if an error occured while closing the directory
0 if no error occured
*/
int directoryValid(const char *path);

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
Starts a child process from the parent process. Stops the parent process. Transforms the child process into a daemon. Sleeps and synchronizes directories. Handles signals.
reads:
source - source directory path
destination - target directory path
interval - sleep time in seconds
recursive - recursive directory synchronization (boolean)
*/
void runDaemon(char *source, char *destination, unsigned int interval, char recursive);

/*
Fills the list of files of directory dir.
reads:
dir - directory stream opened with opendir
writes:
files - list of regular files located in dir
returns:
< 0 if an error occured
0 if no error occured
*/
int listFiles(DIR *dir, list *files);
/*
Fills the lists of files and subdirectories of directory dir.
writes:
dir - directory stream opened with opendir
writes:
files - list of regular files located in dir
subdirs - list of subdirectories located in dir
returns:
< 0 if an error occured
0 if no error occured
*/
int listFilesAndDirectories(DIR *dir, list *files, list *subdirs);

/*
Creates an empty directory.
reads:
path - directory path, absolute or relative to the process' current working directory (cwd)
mode - directory permissions
returns:
-1 if an error occured
0 if no error occured
*/
int createEmptyDirectory(const char *path, mode_t mode);
/*
Recursively removes a directory.
reads:
path - directory path, absolute or relative to the process' current working directory (cwd); must end with '/'
pathLength - path length in bytes
returns:
< 0 if a critical error occured
> 0 if a non-critical error occured
0 if no error occured
*/
int removeDirectoryRecursively(const char *path, const size_t pathLength);

/*
Copies a file. Reads the source file using read function and writes the target file using write function.
reads:
srcFilePath - source file path, absolute or relative to the process' current working directory (cwd)
dstFilePath - target file path, absolute or relative to the process' current working directory (cwd)
dstMode - permissions set on the target file
dstAccessTime - last access time set to the target file
dstModificationTime - last modification time set to the target file
returns:
< 0 if a critical error occured
> 0 if a non-critical error occured
0 if no error occured
*/
int copySmallFile(const char *srcFilePath, const char *dstFilePath, const mode_t dstMode, const struct timespec *dstAccessTime, const struct timespec *dstModificationTime);
/*
Copies a file. Reads the source file from memory mapped using mmap and writes the target file using write function.
reads:
srcFilePath - source file path, absolute or relative to the process' current working directory (cwd)
dstFilePath - target file path, absolute or relative to the process' current working directory (cwd)
dstMode - permissions set on the target file
dstAccessTime - last access time set to the target file
dstModificationTime - last modification time set to the target file
returns:
< 0 if a critical error occured
> 0 if a non-critical error occured
0 if no error occured
*/
int copyBigFile(const char *srcFilePath, const char *dstFilePath, const unsigned long long fileSize, const mode_t dstMode, const struct timespec *dstAccessTime, const struct timespec *dstModificationTime);
/*
Deletes a file.
reads:
path - file path, absolute or relative to the process' current working directory (cwd)
returns:
-1 if an error occured
0 if no error occured
*/
int removeFile(const char *path);

/*
Detects differences and updates files in the target directory. If an inode (index node, a physical file in mass storage) has more than 1 name (hard link) in the source directory, then we copy every hard link as a separate file.
reads:
srcDirPath - source directory path, absolute or relative to the process' current working directory (cwd); must end with '/'
srcDirPathLength - length in bytes of srcDirPath
filesSrc - ordered (sorted) list of files located in source directory
dstDirPath - target directory path, absolute or relative to the process' current working directory (cwd); must end with '/'
dstDirPathLength - length in bytes of dstDirPath
filesDst - in the same order as filesSrc list of files located in target directory
returns:
< 0 if an error occured which prevents from checking all files
> 0 if an error occured which prevents from editing a file
0 if no error occured
*/
int updateDestinationFiles(const char *srcDirPath, const size_t srcDirPathLength, list *filesSrc, const char *dstDirPath, const size_t dstDirPathLength, list *filesDst);
/*
Detects differences and updates subdirectories in the target directory.
reads:
srcDirPath - source directory path, absolute or relative to the process' current working directory (cwd); must end with '/'
srcDirPathLength - length in bytes of srcDirPath
subdirsSrc - ordered (sorted) list of subdirectories located in source directory
dstDirPath - target directory path, absolute or relative to the process' current working directory (cwd); must end with '/'
dstDirPathLength - length in bytes of dstDirPath
subdirsDst - in the same order as subdirsSrc list of subdirectories located in target directory
writes:
isReady - boolean array with length equal to the number of subdirectories in source directory; if i-th subdirectory in list subdirsSrc does not exist in the target directory and:
- creating it is unsuccessful, then isReady[i] == 0
- creating it is successful, then isReady[i] == 1
returns:
< 0 if an error occured which prevents from checking all subdirectories
> 0 if at least 1 error occured which prevents from creating a subdirectory
0 if no error occured
*/
int updateDestinationDirectories(const char *srcDirPath, const size_t srcDirPathLength, list *subdirsSrc, const char *dstDirPath, const size_t dstDirPathLength, list *subdirsDst, char *isReady);

/*
Non-recursively synchronizes the source and target directories.
reads:
sourcePath - source directory path, absolute or relative to the process' current working directory (cwd); must end with '/'
sourcePathLength - length in bytes of sourcePath
destinationPath - target directory path, absolute or relative to the process' current working directory (cwd); must end with '/'
destinationPathLength - length in bytes of destinationPath
returns:
< 0 if an error occured
0 if no error occured
*/
int synchronizeNonRecursively(const char *sourcePath, const size_t sourcePathLength, const char *destinationPath, const size_t destinationPathLength);
/*
Recursively synchronizes the source and target directories.
reads:
sourcePath - source directory path, absolute or relative to the process' current working directory (cwd); must end with '/'
sourcePathLength - length in bytes of sourcePath
destinationPath - target directory path, absolute or relative to the process' current working directory (cwd); must end with '/'
destinationPathLength - length in bytes of destinationPath
returns:
< 0 if an error occured
0 if no error occured
*/
int synchronizeRecursively(const char *sourcePath, const size_t sourcePathLength, const char *destinationPath, const size_t destinationPathLength);
/*
Pointer to a function synchronizing the source and target directories.
*/
typedef int (*synchronizer)(const char *sourcePath, const size_t sourcePathLength, const char *destinationPath, const size_t destinationPathLength);

#endif
