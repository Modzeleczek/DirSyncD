# DirSyncD

## Code remark
I am aware that this applications's code is heavily over-commented. To justify myself, this project was created 2 years ago and was supposed to be a product of teamwork but I wrote it all by myself. Then, I wanted my 'teammates' to know what was going on in the code in case they would be asked to present it to the lecturer. Therefore, I wrote a comment for virtually every line of code and after 2 years, I translated the whole project to English to preserve its consistency.

---
## Operation
DirSyncD is a daemon periodically synchronizing 2 directories. Daemon is a program running as a background process, rather than being under the direct control of an interactive user ([source](https://en.wikipedia.org/wiki/Daemon_(computing)), accessed 21.03.2023).

DirSyncD demands two arguments: source and target paths. If any of the paths is not a directory, the program immediately exits printing an error message. Otherwise, it turns into a daemon. It sleeps for 5 minutes (the time can be changed using an additional option), after which it compares the source and target directories. Directory entries not being regular files are ignored (e.g. directories, symbolic links). If the daemon finds:
- a new file in the source directory and that file is not present in the target directory or
- a file in the source directory having modification time other than its equivalent in the target directory

then the daemon copies the file from the source directory to the target directory. After that, it copies the modification time as well to prevent copying the same file on next wake-up (unless it is changed in either source or target directory). If the daemon finds a file in the target directory which is not present in the source directory, it deletes the file from the target directory. The daemon can be immediately woken up by sending signal SIGUSR1 to it. A comprehensive message about every daemon's action (e.g. falling asleep, waking up, copying or deleting a file) is sent to the system log (/var/log/syslog). Such a message contains the current time.

'-R' additional option enables recursive directory synchronization. In this case, directory entries being directories are not ignored. Notably, if the daemon finds a subdirectory in the target directory which is not present in the source directory, it deletes the subdirectory along with its content.

Small files are copied using read/write system calls and big files using mmap/write (the source file is entirely mapped in memory). Big file threshold for distinguishing between small and big files can be passed as additional option.

---
## Building
DirSyncD only uses Linux system calls and C standard library functions without any external dependencies. To build it, firstly clone this repository. Make sure you have `gcc` (GNU Compiler Collection) and `make` (GNU Make) available. Build the application using the following command:
```
make DirSyncD
```

Compiled executable `DirSyncD` is placed in `./build` directory.

To delete `./build`, use:
```
make clean
```

---
## Running
To learn how DirSyncD exactly works, see `Operation` above.

### Startup parameters
2 essential arguments must be passed to DirSyncD:
- `source_path` - path of the directory from which to copy
- `target_path` - path of the directory to which to copy

It can also receive the following additional options:
- `-i <sleep_time>` - sleep time
- `-R` - recursive directory synchronization
- `-t <big_file_threshold>` - minimal file size to consider it big and copy it using mmap

The startup parameters can be summarized as follows:
```
DirSyncD [-i <sleep_time>] [-R] [-t <big_file_threshold>] source_path target_path
```

### Interacting
A running DirSyncD daemon can only be controlled with signals.

Send signal SIGUSR1 to the daemon process:
- during sleep - to prematurely wake it up.
- during synchronization - to force it to repeat the synchronization immediately after finishing the current one.

Send signal SIGTERM to the daemon process:
- during sleep - to stop it.
- during synchronization - to force it to stop after finishing the current synchronization unless the daemon receives SIGUSR1 during it.
