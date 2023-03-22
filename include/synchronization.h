#ifndef SYNCHRONIZATION_H
#define SYNCHRONIZATION_H

#include "linked_list.h"

#include <stddef.h>

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

#endif // SYNCHRONIZATION_H
