#include "directory.h"
#include "file.h"
#include "path.h"

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stddef.h>

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
  /* Return the correct ending code meaning that the directory exists
  and operating on it does not cause errors. */
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
    /* Set an error code. After that, the program goes
    to the end of the current function. */
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
        /* Copy the directory path as the beginning of its file
        and subdirectory paths. */
        strcpy(subPath, path);
        // Save a pointer to the first subdirectory.
        element *cur = subdirs.first;
        // Recursively remove subdirectories.
        while (cur != NULL)
        {
          /* Append the subdirectory name to its parent directory path.
          Function removeDirectoryRecursively demands that
          the directory path end with '/'. */
          size_t subPathLength = appendSubdirectoryName(subPath, pathLength,
            cur->entry->d_name);
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
  /* If the directory was opened, close it. If an error occured
  and the error code is not negative yet,
  thus any critical error has not occured yet */
  if (dir != NULL && closedir(dir) == -1 && ret >= 0)
    // Set a positive error code indicating a non-critical error.
    ret = 1;
  /* If any critical error did not occur, delete the directory.
  If an error occured */
  if (ret >= 0 && rmdir(path) == -1)
    // Set an error code.
    ret = -6;
  // Return the status code.
  return ret;
}

int listFiles(DIR *dir, list *files)
{
  struct dirent *entry;
  // Initially, set errno to 0.
  errno = 0;
  // Read a directory entry. If no error occured
  while ((entry = readdir(dir)) != NULL)
  {
    /* If the entry is a regular file, add it to the file list.
    If an error occured */
    if (entry->d_type == DT_REG && pushBack(files, entry) < 0)
      /* Interrupt returning an error code because directory entry lists
      must be complete for directory comparison during a synchronization. */
      return -1;
  }
  /* If an error occured while reading a directory entry,
  then readdir returned NULL and set errno to a value not equal to 0. */
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
        /* Interrupt returning an error code because directory entry lists
        must be complete for directory comparison during a synchronization. */
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
    /* Ignore entries of other types (symbolic links, block devices,
    character devices, sockets, etc.). */
  }
  /* if an error occured while reading a directory entry,
  then readdir returned NULL and set errno to a value not equal to 0. */
  if (errno != 0)
    // Return an error code.
    return -3;
  // Return the correct ending code.
  return 0;
}
