#include "path.h"

#include <string.h>

void stringAppend(char *dst, const size_t offset, const char *src)
{
  /* Move offset bytes from the beginning of string dst.
  Starting at the calculated position, insert string src. */
  strcpy(dst + offset, src);
  /* Strcat can be used but it appends src at the end of dst
  and presumably wastes time calculating the length of dst using strlen. */
}

size_t appendSubdirectoryName(char *path, const size_t pathLength,
  const char *subName)
{
  // Append the subdirectory name to source directory path.
  stringAppend(path, pathLength, subName);
  /* Calculate the length of subdirectory path as sum of source directory
  path length and subdirectory name length. */
  size_t subPathLength = pathLength + strlen(subName);
  // Append '/' to the created subdirectory path.
  stringAppend(path, subPathLength, "/");
  // Increment subdirectory path length by 1 because '/' was appended.
  subPathLength += 1;
  // Return created subdirectory path length.
  return subPathLength;
}
