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
DirSyncD only uses Linux system calls and C standard library functions without any external dependencies. To build it, firstly clone this repository. Make sure you have `gcc` (GNU Compiler Collection) available. Then you have two options:
1. If `make` (GNU Make) is available, build the application using the following command:
    ```
    make DirSyncD
    ```

or

2. Permit execution of build.sh Bash script and run it:
    ```
    chmod 755 build.sh
    ./build.sh
    ```

Either way, compiled executable `DirSyncD` is placed in `./build` directory.

To delete `./build`, use:
1.  ```
    make clean
    ```

or

2.  ```
    rm -r ./build
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

---
## Usage example
`DirSyncD` project directory (without `.git` because it has hundreds of files that otherwise would be printed) is located in `~/test`. Empty `DirSyncD_backup` directory will be the target during synchronization.
```
modzel@Modzel-G710:~/test$ tree -a
.
├── DirSyncD
│   ├── build
│   │   ├── DirSyncD
│   │   └── DirSyncD.o
│   ├── build.sh
│   ├── DirSyncD.c
│   ├── DirSyncD.h
│   ├── .gitignore
│   ├── LICENSE
│   ├── Makefile
│   ├── README.md
│   └── .vscode
│       └── settings.json
└── DirSyncD_backup

4 directories, 10 files
```

`Diff` shows that all entries of `DirSyncD` are only there and not in `DirSyncD_backup`.
```
modzel@Modzel-G710:~/test$ diff -a -q -r DirSyncD DirSyncD_backup
Only in DirSyncD: build
Only in DirSyncD: build.sh
Only in DirSyncD: DirSyncD.c
Only in DirSyncD: DirSyncD.h
Only in DirSyncD: .gitignore
Only in DirSyncD: LICENSE
Only in DirSyncD: Makefile
Only in DirSyncD: README.md
Only in DirSyncD: .vscode
```

On start, `DirSyncD` prints PID of the daemon process.
```
modzel@Modzel-G710:~/test$ ./DirSyncD/build/DirSyncD -R -i 60 DirSyncD/ DirSyncD_backup/
PID of the child process: 26145
```

The daemon logs a start of sleep in `/var/log/syslog`.
```
Mar 22 00:18:33 Modzel-G710 DirSyncD[26145]: falling asleep
```

After 60 seconds of sleep, the daemon wakes up and performs a synchronization. It logs every operation's details and status code.
```
Mar 22 00:19:33 Modzel-G710 DirSyncD[26145]: waking up; slept for 60 s
Mar 22 00:19:33 Modzel-G710 DirSyncD[26145]: copying file /home/modzel/test/DirSyncD/.gitignore to directory /home/modzel/test/DirSyncD_backup/; 0
Mar 22 00:19:33 Modzel-G710 DirSyncD[26145]: copying file /home/modzel/test/DirSyncD/DirSyncD.c to directory /home/modzel/test/DirSyncD_backup/; 0
Mar 22 00:19:33 Modzel-G710 DirSyncD[26145]: copying file /home/modzel/test/DirSyncD/DirSyncD.h to directory /home/modzel/test/DirSyncD_backup/; 0
Mar 22 00:19:33 Modzel-G710 DirSyncD[26145]: copying file /home/modzel/test/DirSyncD/LICENSE to directory /home/modzel/test/DirSyncD_backup/; 0
Mar 22 00:19:33 Modzel-G710 DirSyncD[26145]: copying file /home/modzel/test/DirSyncD/Makefile to directory /home/modzel/test/DirSyncD_backup/; 0
Mar 22 00:19:33 Modzel-G710 DirSyncD[26145]: copying file /home/modzel/test/DirSyncD/README.md to directory /home/modzel/test/DirSyncD_backup/; 0
Mar 22 00:19:33 Modzel-G710 DirSyncD[26145]: copying file /home/modzel/test/DirSyncD/build.sh to directory /home/modzel/test/DirSyncD_backup/; 0
Mar 22 00:19:33 Modzel-G710 DirSyncD[26145]: creating directory /home/modzel/test/DirSyncD_backup/.vscode; 0
Mar 22 00:19:33 Modzel-G710 DirSyncD[26145]: creating directory /home/modzel/test/DirSyncD_backup/build; 0
Mar 22 00:19:33 Modzel-G710 DirSyncD[26145]: copying file /home/modzel/test/DirSyncD/.vscode/settings.json to directory /home/modzel/test/DirSyncD_backup/.vscode/; 0
Mar 22 00:19:33 Modzel-G710 DirSyncD[26145]: copying file /home/modzel/test/DirSyncD/build/DirSyncD to directory /home/modzel/test/DirSyncD_backup/build/; 0
Mar 22 00:19:33 Modzel-G710 DirSyncD[26145]: copying file /home/modzel/test/DirSyncD/build/DirSyncD.o to directory /home/modzel/test/DirSyncD_backup/build/; 0
Mar 22 00:19:33 Modzel-G710 DirSyncD[26145]: finishing synchronization; 0
Mar 22 00:19:33 Modzel-G710 DirSyncD[26145]: falling asleep
```

After the synchronization, `diff` shows nothing.
```
modzel@Modzel-G710:~/test$ diff -a -q -r DirSyncD DirSyncD_backup
```

To test the synchronization again, in the source directory we edit `DirSyncD.c`, create `new_file` and delete `README.md`.
```
modzel@Modzel-G710:~/test$ diff -a -q -r DirSyncD DirSyncD_backup
Files DirSyncD/DirSyncD.c and DirSyncD_backup/DirSyncD.c differ
Only in DirSyncD: new_file
Only in DirSyncD_backup: README.md
```

We send SIGUSR1 signal to the daemon.
```
modzel@Modzel-G710:~/test$ kill -SIGUSR1 26145
```

It prematurely wakes up and performs an incremental synchronization.
```
Mar 22 00:20:26 Modzel-G710 DirSyncD[26145]: waking up; slept for 54 s
Mar 22 00:20:26 Modzel-G710 DirSyncD[26145]: writing /home/modzel/test/DirSyncD/DirSyncD.c to /home/modzel/test/DirSyncD_backup/DirSyncD.c; 0
Mar 22 00:20:26 Modzel-G710 DirSyncD[26145]: deleting file /home/modzel/test/DirSyncD_backup/README.md; 0
Mar 22 00:20:26 Modzel-G710 DirSyncD[26145]: copying file /home/modzel/test/DirSyncD/new_file to directory /home/modzel/test/DirSyncD_backup/; 0
Mar 22 00:20:26 Modzel-G710 DirSyncD[26145]: finishing synchronization; 0
Mar 22 00:20:26 Modzel-G710 DirSyncD[26145]: falling asleep
```

Finally, we send SIGTERM to the daemon.
```
modzel@Modzel-G710:~/test$ kill -SIGTERM 26145
```

It wakes up and stops.
```
Mar 22 00:20:45 Modzel-G710 DirSyncD[26145]: waking up; slept for 19 s
Mar 22 00:20:45 Modzel-G710 DirSyncD[26145]: stopping; 0
```
