# DirSyncD

## Operation
DirSyncD is a daemon periodically synchronizing 2 directories. Daemon is a program running as a background process, rather than being under the direct control of an interactive user ([source](https://en.wikipedia.org/wiki/Daemon_(computing)), accessed 21.03.2023).

DirSyncD demands two arguments: source and target paths. If any of the paths is not a directory, the program immediately exits printing an error message. Otherwise, it turns into a daemon. It sleeps for 5 minutes (the time can be changed using an additional option), after which it compares the source and target directories. Directory entries not being regular files are ignored (e.g. directories, symbolic links). If the daemon finds:
- a new file in the source directory and that file is not present in the target directory or
- a file in the source directory having modification time other than its equivalent in the target directory

then the daemon copies the file from the source directory to the target directory. After that, it copies the modification time as well to prevent copying the same file on next wake-up (unless it is changed in either source or target directory). If the daemon finds a file in the target directory which is not present in the source directory, it deletes the file from the target directory. The daemon can be immediately woken up by sending signal SIGUSR1 to it. A comprehensive message about every daemon's action (e.g. falling asleep, waking up, copying or deleting a file) is sent to the system log (/var/log/syslog). Such a message contains the current time.

'-R' additional option enables recursive directory synchronization. In this case, directory entries being directories are not ignored. Notably, if the daemon finds a subdirectory in the target directory which is not present in the source directory, it deletes the subdirectory along with its content.

Small files are copied using read/write system calls and big files using mmap/write (the source file is entirely mapped in memory). Big file threshold for distinguishing between small and big files can be passed as additional option.

---
## Usage
Essential arguments:
- `source_path` - path of the directory from which we copy
- `target_path` - path of the directory to which we copy

Additional options:
- `-i <sleep_time>` - sleep time
- `-R` - recursive directory synchronization
- `-t <big_file_threshold>` - minimal file size to consider it big and copy it using mmap

Usage:
```
DirSyncD [-i <sleep_time>] [-R] [-t <big_file_threshold>] source_path target_path
```

Send signal SIGUSR1 to the daemon:
- during sleep - to prematurely wake it up.
- during synchronization - to force it to repeat the synchronization immediately after finishing the current one.

Send signal SIGTERM to the daemon:
- during sleep - to stop it.
- during synchronization - to force it to stop after finishing the current synchronization unless the daemon receives SIGUSR1 during it.
