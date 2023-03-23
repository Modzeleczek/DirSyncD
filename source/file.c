#include "file.h"

#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <stddef.h>

// Size of the buffer used to copy files.
#define BUFFERSIZE 4096

int copySmallFile(const char *srcFilePath, const char *dstFilePath,
  const mode_t dstMode, const struct timespec *dstAccessTime,
  const struct timespec *dstModificationTime)
{
  // Initially, set status code indicating no error.
  int ret = 0, in = -1, out = -1;
  /* Open the source file for reading and save its descriptor.
  If an error occured */
  if ((in = open(srcFilePath, O_RDONLY)) == -1)
    /* Set an error code. After that, the program immediately goes to the end
    of the current function. */
    ret = -1;
  /* Open the target file for writing. If it does not exist,
  create it with empty permissions. Otherwise, clear it.
  Save its descriptor. If an error occured */
  else if ((out = open(dstFilePath, O_WRONLY | O_CREAT | O_TRUNC, 0000)) == -1)
    // Set an error code.
    ret = -2;
  // Set the target file dstMode permissions. If an error occured
  else if (fchmod(out, dstMode) == -1)
    // Set an error code.
    ret = -3;
  else
  {
    /* (page 124) Send an advice to the kernel that the source file
    will be read sequentially so it should be loaded in advance. */
    if (posix_fadvise(in, 0, 0, POSIX_FADV_SEQUENTIAL) == -1)
      /* Set a non-critical error code because the source file can be read
      even without the advice but less effectively. */
      ret = 1;
    char *buffer = NULL;
    /* Optimal buffer size for input/output operations on a file can be checked
    in its metadata read using stat but we use a predefined size. */
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
        /* While numbers of remaining bytes and bytes read
        in the current iteration are non-zero. */
        while (remainingBytes != 0 && (bytesRead =
          read(in, position, remainingBytes)) != 0)
        {
          // If an error occured in function read.
          if (bytesRead == -1)
          {
            /* If function read was interrupted by receiving a signal. SIGUSR1
            and SIGTERM are blocked for the synchronization so those signals
            cannot cause this error. */
            if (errno == EINTR)
              // Retry reading.
              continue;
            // If other error occured
            // Set an error code.
            ret = -5;
            // BUFFERSIZE - BUFFERSIZE == 0 so the second loop is not executed
            remainingBytes = BUFFERSIZE;
            /* Set 0 so condition if (bytesRead == 0) breaks
            external loop while (1). */
            bytesRead = 0;
            // Break the inner loop.
            break;
          }
          /* Decrease the number of remaining bytes by the number of bytes
          read in the current iteration and */
          remainingBytes -= bytesRead;
          // move the position in the buffer.
          position += bytesRead;
        }
        position = buffer; // page 48
        /* Save the total number of read bytes that is always less
        or equal to the buffer size. */
        remainingBytes = BUFFERSIZE - remainingBytes;
        ssize_t bytesWritten;
        /* While numbers of remaining bytes and bytes written
        in the current iteration are non-zero. */
        while (remainingBytes != 0 && (bytesWritten =
          write(out, position, remainingBytes)) != 0)
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
            /* Set 0 so condition if (bytesRead == 0) breaks
            external loop while (1). */
            bytesRead = 0;
            // Break the inner loop.
            break;
          }
          /* Decrease the number of remaining bytes by the number of bytes
          written in the current iteration and */
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
        /* Must be set after writing the target file ends because writing sets
        the modification time to the current operating system time.
        Set the times of the target file. If an error occured */
        if (futimens(out, times) == -1)
          // Set an error code.
          ret = -7;
      }
    }
  }
  // If the source file was opened, close it. If an error occured
  if (in != -1 && close(in) == -1)
    /* Set an error code. Do not consider it a not-critical error
    because every unclosed file occupies a descriptor
    of which the process can have only a finite number. */
    ret = -8;
  // If the target file was opened, close it. If an error occured
  if (out != -1 && close(out) == -1)
    // Set an error code.
    ret = -9;
  // Return the status code.
  return ret;
}

int copyBigFile(const char *srcFilePath, const char *dstFilePath,
  const unsigned long long fileSize, const mode_t dstMode,
  const struct timespec *dstAccessTime,
  const struct timespec *dstModificationTime)
{
  // Initially, set status code indicating no error.
  int ret = 0, in = -1, out = -1;
  /* Open the source file for reading and save its descriptor.
  If an error occured */
  if ((in = open(srcFilePath, O_RDONLY)) == -1)
    /* Set an error code. After that, the program immediately goes
    to the end of the current function. */
    ret = -1;
  /* Open the target file for writing. If it does not exist, create it
  with empty permissions. Otherwise, clear it. Save its descriptor.
  If an error occured */
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
      /* (page 121) Send an advice to the kernel that the source file
      will be read sequentially so it should be loaded in advance.
      If an error occured */
      if (madvise(map, fileSize, MADV_SEQUENTIAL) == -1)
        /* Set a non-critical error code because the source file can be read
        even without the advice but less effectively. */
        ret = 1;
      char *buffer = NULL;
      /* Optimal buffer size for input/output operations on a file can be
      checked in its metadata read using stat but we use a predefined size. */
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
        /* Cannot be (b < fileSize - BUFFERSIZE) because b and fileSize are
        of unsigned type so if fileSize < BUFFERSIZE and we subtract,
        then we have an overflow. */
        for (b = 0; b + BUFFERSIZE < fileSize; b += BUFFERSIZE)
        {
          /* Copy BUFFERSIZE (size of the buffer) bytes from the mapped memory
          to the buffer. */
          memcpy(buffer, map + b, BUFFERSIZE);
          // The algorithm below is on page 48.
          position = buffer;
          /* Save the total number of bytes remaining to be written
          which is always equal to buffer size. */
          remainingBytes = BUFFERSIZE;
          /* While numbers of remaining bytes and bytes written
          in the current iteration are non-zero. */
          while (remainingBytes != 0 && (bytesWritten =
            write(out, position, remainingBytes)) != 0)
          {
            // If an error occured in function write.
            if (bytesWritten == -1)
            {
              /* If function write was interrupted by receiving a signal.
              SIGUSR1 and SIGTERM are blocked for the synchronization
              so those signals cannot cause this error. */
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
            /* Decrease the number of remaining bytes by the number
            of bytes written in the current iteration and */
            remainingBytes -= bytesWritten;
            // move the position in the buffer.
            position += bytesWritten;
          }
        }
        // If no error occured while copying
        if (ret >= 0)
        {
          /* Save the number of bytes located at the end of the source file
          which did not fit into one full buffer. */
          remainingBytes = fileSize - b;
          // Copy them from the mapped memory to the buffer.
          memcpy(buffer, map + b, remainingBytes);
          // Save the position of the first byte in the buffer.
          position = buffer;
          /* While numbers of remaining bytes and bytes written
          in the current iteration are non-zero. */
          while (remainingBytes != 0 && (bytesWritten =
            write(out, position, remainingBytes)) != 0)
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
            /* Decrease the number of remaining bytes by the number
            of bytes written in the current iteration and */
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
          const struct timespec times[2] =
            {*dstAccessTime, *dstModificationTime};
          /* Must be set after writing the target file ends because writing sets
          the modification time to the current operating system time.
          Set the times of the target file. If an error occured */
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
    /* Set an error code. Do not consider it a not-critical error
    because every unclosed file occupies a descriptor of which the process
    can have only a finite number. */
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
  /* Unlink is used only for removing files. A directory we remove using rmdir
  but first we have to empty it. Remove the file and return a status code. */
  return unlink(path);
}
