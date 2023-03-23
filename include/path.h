#ifndef PATH_H
#define PATH_H

#include <stddef.h>

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
size_t appendSubdirectoryName(char *path, const size_t pathLength,
  const char *subName);

#endif // PATH_H
