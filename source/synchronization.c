#include "directory.h"
#include "file.h"
#include "path.h"
#include "synchronization.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <syslog.h>

// 'extern' - a global variable declared in a different .c file
/* Big file threshold. If the file size is lesser than threshold,
then during copying the file is considered small, otherwise big. */
extern unsigned long long threshold;

int updateDestinationFiles(const char *srcDirPath,
  const size_t srcDirPathLength, list *filesSrc,
  const char *dstDirPath, const size_t dstDirPathLength, list *filesDst)
{
  char *srcFilePath = NULL, *dstFilePath = NULL;
  /* Reserve memory for paths of files in the source directory.
  If an error occured */
  if ((srcFilePath = malloc(sizeof(char) * PATH_MAX)) == NULL)
    // Return an error code.
    return -1;
  /* Reserve memory for paths of files in the target directory.
  If an error occured */
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
      syslog(LOG_INFO, "deleting file %s; %i\n", dstFilePath, status);
      // If an error occured
      if (status != 0)
        /* Set a positive error code to indicate partial synchronization
        but do not break the loop. */
        ret = 1;
      // Move the pointer to the next target file.
      curD = curD->next;
    }
    else
    {
      // Append source file name to its parent directory path.
      stringAppend(srcFilePath, srcDirPathLength, srcFileName);
      /* Read source file metadata. If an error occured, the source file
      is unavailable and will not be able to be copied when comparison < 0. */
      if (stat(srcFilePath, &srcFile) == -1)
      {
        // If the source file is less than the target file in the order
        if (comparison < 0)
        {
          /* In the log, write a message about unsuccessful copying.
          The status code written to errno by stat is a positive number. */
          syslog(LOG_INFO, "copying file %s to directory %s; %i\n",
            srcFilePath, dstDirPath, errno);
          // Set an error code.
          ret = 2;
        }
        // If the source file is equal to the target file in the order
        else
        {
          // In the log, save a message about unsuccessful metadata reading.
          syslog(LOG_INFO, "reading metadata of source file %s; %i\n",
            srcFilePath, errno);
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
          /* Copy it as a small file. Copy permissions and modification time
          of the source file to the target file. */
          status = copySmallFile(srcFilePath, dstFilePath, srcFile.st_mode,
            &srcFile.st_atim, &srcFile.st_mtim);
        /* If the source file is bigger or the same size
        as the big file threshold */
        else
          // Copy it as a big file.
          status = copyBigFile(srcFilePath, dstFilePath, srcFile.st_size,
            srcFile.st_mode, &srcFile.st_atim, &srcFile.st_mtim);
        // In the log, write a message about copying.
        syslog(LOG_INFO, "copying file %s to directory %s; %i\n",
          srcFilePath, dstDirPath, status);
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
        /* Read target file metadata. If an error occured, the target file
        is unavailable and we will not be able to compare modification times. */
        if (stat(dstFilePath, &dstFile) == -1)
        {
          // In the log, save a message about unsuccessful metadata reading.
          syslog(LOG_INFO, "reading metadata of target file %s; %i\n",
            dstFilePath, errno);
          // Set an error code.
          ret = 5;
        }
        /* If metadata was read correctly and the target file has
        other modification time than the source file
        (earlier - target is outdated or later - target was modified) */
        else if (srcFile.st_mtim.tv_sec != dstFile.st_mtim.tv_sec ||
          srcFile.st_mtim.tv_nsec != dstFile.st_mtim.tv_nsec)
        {
          // Copy the source file to an existing target file.
          // If the source file is smaller than the big file threshold
          if (srcFile.st_size < threshold)
            // Copy it as a small file.
            status = copySmallFile(srcFilePath, dstFilePath, srcFile.st_mode,
              &srcFile.st_atim, &srcFile.st_mtim);
          /* If the source file is bigger or the same size
          as the big file threshold */
          else
            // Copy it as a big file.
            status = copyBigFile(srcFilePath, dstFilePath, srcFile.st_size,
              srcFile.st_mode, &srcFile.st_atim, &srcFile.st_mtim);
          // In the log, write a message about copying.
          syslog(LOG_INFO, "writing %s to %s; %i\n",
            srcFilePath, dstFilePath, status);
          // If an error occured
          if (status != 0)
            // Set an error code.
            ret = 6;
        }
        /* After copying, copy permissions but if the file was not copied,
        check if both files have equal permissions. If the files have
        different permissions */
        else if (srcFile.st_mode != dstFile.st_mode)
        {
          /* Copy permissions from the source file to the target file.
          If an error occured */
          if (chmod(dstFilePath, srcFile.st_mode) == -1)
          {
            /* Set status code not equal to 0 because errno has
            value not equal to 0. */
            status = errno;
            // Set an error code.
            ret = 7;
          }
          // If no error occured
          else
            // Set status code equal to 0.
            status = 0;
          // In the log, write a message about copying permissions.
          syslog(LOG_INFO, "copying permissions of file %s to %s; %i\n",
            srcFilePath, dstFilePath, status);
        }
        // Move the pointer to the next source file.
        curS = curS->next;
        // Move the pointer to the next target file.
        curD = curD->next;
      }
    }
  }
  /* If any remaining files exist in the target directory, remove them because
  they do not exist in the source directory.
  Start removing at the file currently pointed to by curD. */
  while (curD != NULL)
  {
    char *dstFileName = curD->entry->d_name;
    // Append target file name to its parent directory path.
    stringAppend(dstFilePath, dstDirPathLength, dstFileName);
    // Remove the target file.
    status = removeFile(dstFilePath);
    // In the log, write a message about removal.
    syslog(LOG_INFO, "deleting file %s; %i\n", dstFilePath, status);
    // If an error occured
    if (status != 0)
      // Set an error code.
      ret = 8;
    // Move the pointer to the next target file.
    curD = curD->next;
  }
  /* If any remaining files exist in the source directory, copy them because
  they do not exist in the target directory.
  Start copying at the file currently pointed to by curS. */
  while (curS != NULL)
  {
    char *srcFileName = curS->entry->d_name;
    // Append source file name to its parent directory path.
    stringAppend(srcFilePath, srcDirPathLength, srcFileName);
    // Read source file metadata. If an error occured
    if (stat(srcFilePath, &srcFile) == -1)
    {
      // In the log, write a message about unsuccessful copying.
      syslog(LOG_INFO, "copying file %s to directory %s; %i\n",
        srcFilePath, dstDirPath, errno);
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
        status = copySmallFile(srcFilePath, dstFilePath, srcFile.st_mode,
          &srcFile.st_atim, &srcFile.st_mtim);
      // If the source file is bigger or the same size as the big file threshold
      else
        // Copy it as a big file.
        status = copyBigFile(srcFilePath, dstFilePath, srcFile.st_size,
          srcFile.st_mode, &srcFile.st_atim, &srcFile.st_mtim);
      // In the log, write a message about copying.
      syslog(LOG_INFO, "copying file %s to directory %s; %i\n",
        srcFilePath, dstDirPath, status);
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

int updateDestinationDirectories(const char *srcDirPath,
  const size_t srcDirPathLength, list *subdirsSrc, const char *dstDirPath,
  const size_t dstDirPathLength, list *subdirsDst, char *isReady)
{
  char *srcSubdirPath = NULL, *dstSubdirPath = NULL;
  /* Reserve memory for paths of subdirectories in the source directory
    (hereinafter referred to as source subdirectories). If an error occured */
  if ((srcSubdirPath = malloc(sizeof(char) * PATH_MAX)) == NULL)
    // Return an error code.
    return -1;
  /* Reserve memory for paths of subdirectories in the target directory
    (hereinafter referred to as target subdirectories). If an error occured */
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
    char *srcSubdirName = curS->entry->d_name,
      *dstSubdirName = curD->entry->d_name;
    // Compare source and target subdirectory names in lexicographic order.
    int comparison = strcmp(srcSubdirName, dstSubdirName);
    /* If the source subdirectory is greater than the target subdirectory
    in the order */
    if (comparison > 0)
    {
      /* Append target subdirectory name to its parent directory path.
      Function removeDirectoryRecursively demands that the directory path end
      with '/'. */
      size_t length = appendSubdirectoryName(dstSubdirPath, dstDirPathLength,
        dstSubdirName);
      // Recursively remove the target subdirectory.
      status = removeDirectoryRecursively(dstSubdirPath, length);
      // In the log, write a message about removal.
      syslog(LOG_INFO, "deleting directory %s; %i\n", dstSubdirPath, status);
      // If an error occured
      if (status != 0)
        /* Set a positive error code to indicate partial synchronization
        but do not break the loop. */
        ret = 1;
      // Move the pointer to the next target subdirectory.
      curD = curD->next;
    }
    else
    {
      // Append source subdirectory name to its parent directory path.
      stringAppend(srcSubdirPath, srcDirPathLength, srcSubdirName);
      /* Read source subdirectory metadata. If an error occured,
      the source subdirectory is unavailable and will not be able to be created
      when comparison < 0. Even if we created it, we would not be able
      to synchronize it. */
      if (stat(srcSubdirPath, &srcSubdir) == -1)
      {
        /* If the source subdirectory is less than the target subdirectory
        in the order */
        if (comparison < 0)
        {
          /* In the log, write a message about unsuccessful copying.
          The status code written to errno by stat is a positive number. */
          syslog(LOG_INFO, "creating directory %s; %i\n", dstSubdirPath, errno);
          /* Indicate that the subdirectory is unready for synchronization
          because it does not exist. */
          isReady[i++] = 0;
          // Set an error code.
          ret = 2;
        }
        else
        {
          // In the log, save a message about unsuccessful metadata reading.
          syslog(LOG_INFO, "reading metadata of source directory %s; %i\n",
            srcSubdirPath, errno);
          /* If we did not manage to check if source and target subdirectories
          have equal permissions, assume that they do.
          Indicate that the subdirectory is ready for synchronization. */
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
      /* If the source subdirectory is less than the target subdirectory
      in the order */
      if (comparison < 0)
      {
        // Append source subdirectory name to the target directory path.
        stringAppend(dstSubdirPath, dstDirPathLength, srcSubdirName);
        /* In the target directory, create a subdirectory named the same
        as the source subdirectory. Copy permissions but do not copy
        modification time because we ignore it during synchronization -
        all subdirectories are browsed to detect file changes. */
        status = createEmptyDirectory(dstSubdirPath, srcSubdir.st_mode);
        // In the log, write a meesage about creation.
        syslog(LOG_INFO, "creating directory %s; %i\n", dstSubdirPath, status);
        // If an error occured
        if (status != 0)
        {
          /* Indicate that the subdirectory is unready for synchronization
          because it does not exist. */
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
      /* If the source subdirectory is equal to the target subdirectory
      in the order */
      else
      {
        /* Indicate that the subdirectory is ready for synchronization
        even if permission comparison is unsuccessful. */
        isReady[i++] = 1;
        // Append target subdirectory name to its parent directory path.
        stringAppend(dstSubdirPath, dstDirPathLength, dstSubdirName);
        /* Read target subdirectory metadata. If an error occured,
        the target subdirectory is unavailable and we will not be able
        to compare permissions. */
        if (stat(dstSubdirPath, &dstSubdir) == -1)
        {
          // In the log, save a message about unsuccessful metadata reading.
          syslog(LOG_INFO, "reading metadata of target directory %s; %i\n",
            dstSubdirPath, errno);
          // Set an error code.
          ret = 5;
        }
        /* Ignore subdirectory modification time (it changes on file creation
        and deletion in the subdirectory). */
        /* If metadata was read correctly and source and target directories
        have different permissions */
        else if (srcSubdir.st_mode != dstSubdir.st_mode)
        {
          /* Copy permissions from source to target subdirectory.
          If an error occured */
          if (chmod(dstSubdirPath, srcSubdir.st_mode) == -1)
          {
            /* Set status code not equal to 0 because errno has value
            not equal to 0. */
            status = errno;
            // Set an error code.
            ret = 6;
          }
          // If no error occured
          else
            // Set status code equal to 0.
            status = 0;
          // In the log, write a message about copying permissions.
          syslog(LOG_INFO, "copying permissions of directory %s to %s; %i\n",
            srcSubdirPath, dstSubdirPath, status);
        }
        // Move the pointer to the next source subdirectory.
        curS = curS->next;
        // Move the pointer to the next target subdirectory.
        curD = curD->next;
      }
    }
  }
  /* If any remaining subdirectories exist in the target directory,
  remove them because they do not exist in the source directory.
  Start removing at the subdirectory currently pointed to by curD. */
  while (curD != NULL)
  {
    char *dstSubdirName = curD->entry->d_name;
    // Append target subdirectory name to its parent directory path.
    size_t length = appendSubdirectoryName(dstSubdirPath, dstDirPathLength,
      dstSubdirName);
    // Recursively remove the target subdirectory.
    status = removeDirectoryRecursively(dstSubdirPath, length);
    // In the log, write a message about removal.
    syslog(LOG_INFO, "deleting directory %s; %i\n", dstSubdirPath, status);
    // If an error occured
    if (status != 0)
      // Set an error code.
      ret = 7;
    // Move the pointer to the next target subdirectory.
    curD = curD->next;
  }
  /* If any remaining subdirectories exist in the source directory,
  copy them because they do not exist in the target directory.
  Start copying at the file currently pointed to by curS. */
  while (curS != NULL)
  {
    char *srcSubdirName = curS->entry->d_name;
    // Append source subdirectory name to its parent directory path.
    stringAppend(srcSubdirPath, srcDirPathLength, srcSubdirName);
    // Read source subdirectory metadata. If an error occured
    if (stat(srcSubdirPath, &srcSubdir) == -1)
    {
      // In the log, write a message about unsuccessful creation.
      syslog(LOG_INFO, "creating directory %s; %i\n", dstSubdirPath, errno);
      /* Indicate that the subdirectory is unready for synchronization
      because it does not exist. */
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
    /* In the target directory, create a subdirectory named the same
    as the source subdirectory and copy permissions. */
    status = createEmptyDirectory(dstSubdirPath, srcSubdir.st_mode);
    // In the log, write a message about creation.
    syslog(LOG_INFO, "creating directory %s; %i\n", dstSubdirPath, status);
    // If an error occured
    if (status != 0)
    {
      /* Indicate that the subdirectory is unready for synchronization
      because it does not exist. */
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

int synchronizeNonRecursively(
  const char *sourcePath, const size_t sourcePathLength,
  const char *destinationPath, const size_t destinationPathLength)
{
  // Initially, set status code indicating no error.
  int ret = 0;
  DIR *dirS = NULL, *dirD = NULL;
  // Open the source directory. If an error occured
  if ((dirS = opendir(sourcePath)) == NULL)
    /* Set status code indicating an error. After that, the program
    immediately goes to the end of the current function. */
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
      /* Check compliance and if needed, update target directory files.
      If an error occured */
      if (updateDestinationFiles(sourcePath, sourcePathLength, &filesS,
        destinationPath, destinationPathLength, &filesD) != 0)
        // Set status code indicating an error.
        ret = -5;
    }
    // Clear the source directory file list.
    clear(&filesS);
    // Clear the target directory file list.
    clear(&filesD);
  }
  // If an error occured somewhere, go here.
  /* Objects of type dirent, which we read from the directory using readdir,
  are removed from memory on calling closedir. Therefore, we cannot close
  the directory until we finish using dirent objects.
  If the source directory was opened */
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

int synchronizeRecursively(
  const char *sourcePath, const size_t sourcePathLength,
  const char *destinationPath, const size_t destinationPathLength)
{
  // Initially, set status code indicating no error.
  int ret = 0;
  DIR *dirS = NULL, *dirD = NULL;
  // Open the source directory. If an error occured
  if ((dirS = opendir(sourcePath)) == NULL)
    /* Set status code indicating an error. After that, the program
    immediately goes to the end of the current function. */
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
    /* Fill the source directory file and subdirectory lists.
    If an error occured */
    if (listFilesAndDirectories(dirS, &filesS, &subdirsS) < 0)
      // Set status code indicating an error.
      ret = -3;
    /* Fill the target directory file and subdirectory lists.
    If an error occured */
    else if (listFilesAndDirectories(dirD, &filesD, &subdirsD) < 0)
      // Set status code indicating an error.
      ret = -4;
    else
    {
      // Sort the source directory file list.
      listMergeSort(&filesS);
      // Sort the target directory file list.
      listMergeSort(&filesD);
      /* Check compliance and if needed, update target directory files.
      If an error occured */
      if (updateDestinationFiles(sourcePath, sourcePathLength, &filesS,
        destinationPath, destinationPathLength, &filesD) != 0)
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
      /* Set i-th cell of array isReady to 1 if i-th source subdirectory exists
      or will be correctly created in the target directory
      by function updateDestinationDirectories so it
      will be ready for recursive synchronization. */
      char *isReady = NULL;
      /* Reserve memory for an array with size equal to the number
      of source subdirectories. If an error occured */
      if ((isReady = malloc(sizeof(char) * subdirsS.count)) == NULL)
        // Set status code indicating an error.
        ret = -6;
      else
      {
        /* Check compliance and if needed, update target directory
        subdirectories. Fill array isReady. If an error occured */
        if (updateDestinationDirectories(sourcePath, sourcePathLength,
          &subdirsS, destinationPath, destinationPathLength, &subdirsD, isReady)
          != 0)
          // Set status code indicating an error.
          ret = -7;
        /* Do not clear source subdirectory list yet because
        function synchronizeRecursively will be recursively called
        on subdirectories from that list. */
        // Clear target subdirectory list.
        clear(&subdirsD);

        char *nextSourcePath = NULL, *nextDestinationPath = NULL;
        // Reserve memory for source subdirectory paths. If an error occured
        if ((nextSourcePath = malloc(sizeof(char) * PATH_MAX)) == NULL)
          // Set status code indicating an error.
          ret = -8;
        // Reserve memory for target subdirectory paths. If an error occured
        else if ((nextDestinationPath = malloc(sizeof(char) * PATH_MAX))
          == NULL)
          // Set status code indicating an error.
          ret = -9;
        else
        {
          /* Copy the source directory path as the beginning
          of its subdirectory paths. */
          strcpy(nextSourcePath, sourcePath);
          /* Copy the target directory path as the beginning
          of its subdirectory paths. */
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
              size_t nextSourcePathLength = appendSubdirectoryName(
                nextSourcePath, sourcePathLength, curS->entry->d_name);
              // Create the target subdirectory path and save its length.
              size_t nextDestinationPathLength = appendSubdirectoryName(
                nextDestinationPath, destinationPathLength,
                curS->entry->d_name);
              // Recursively synchronize subdirectories. If an error occured
              if (synchronizeRecursively(nextSourcePath, nextSourcePathLength,
                nextDestinationPath, nextDestinationPathLength) < 0)
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
    /* If reserving memory for isReady failed, the list subdirsD
    of target subdirectories was not cleared. If it contains any nodes */
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
