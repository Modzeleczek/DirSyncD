#ifndef FILE_H
#define FILE_H

#include <sys/stat.h>

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

#endif // FILE_H
