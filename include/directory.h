#ifndef DIRECTORY_H
#define DIRECTORY_H

#include "linked_list.h"

#include <dirent.h>
#include <sys/stat.h>

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

#endif // DIRECTORY_H
