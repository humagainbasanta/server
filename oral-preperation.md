# Oral Preparation: CSAP Project 2026

## Requirements analysis (from document.pdf)

- Build a Linux C client-server system over sockets that manages virtual home directories under a server root directory.
- Server startup: `./Server <root_directory> <IP> <port>` (IP/port default 127.0.0.1/8080). If root does not exist, create it.
- Client startup: `./Client <IP> <port>` (defaults as above).
- User management: `create_user <username> <permissions_octal>` creates home dir, sets logical owner, sets home permissions; all users share the same GROUP; no password creation; can use `adduser --disabled-password` via fork/exec; minimize time running as root if server uses sudo.
- Session management: `login <username>` authenticates user (no password), sets session owner for subsequent ops.
- File operations after login:
  - `create <path> <permissions>` with `-d` for directory creation.
  - `chmod <path> <permissions>`.
  - `move <path1> <path2>`.
  - `upload <client_path> <server_path>`; `-b` for background transfer; client prints completion notice.
  - `download <server_path> <client_path>`; `-b` for background transfer; client prints completion notice.
  - `cd <path>`.
  - `list [<path>]` prints entries with permissions and logical size; can list other users' dirs if group perms allow; still inside server root.
  - `read <path>` with optional `-o set=<num>` offset; server sends file contents; client prints to stdout.
  - `write <path>` with optional `-o set=<num>`; client reads stdin; server writes/creates file with perms 0700 if new.
  - `delete <path>`.
- Exit:
  - Server `exit` terminates immediately.
  - Client `exit` only terminates if no background transfers; otherwise warn and continue.
- Path handling: all commands accept absolute/relative paths and `.`/`..`.
- Sandboxing: users can never access outside server root or outside their home (except `list` can traverse other users’ homes within root). OTHER permissions unused.
- Concurrency: multiple clients in parallel; protect concurrent file access (reads vs writes, delete as write). Handle race conditions and zombies.
- Prohibited: `system()` and `popen()`; cannot exec shell tools like `cat`, `ls`, `mv`, `rm` to implement functionality (only allowed tools like `adduser`). Use syscalls taught in class.
- Modularity required: split into multiple modules (network, session, transfer, utils, etc.).
- Bonus for 26-30: `transfer_request <file> <dest_user>` with live notification, accept/reject, and blocking sender if dest not online.
- Submission: README with compile/run/command usage; script/Makefile to build; compile on Ubuntu 24.04; no external libraries; originality required.

## Potential oral questions with answers

1) Q: Describe the overall architecture of your system.
   A: It is a client-server system using TCP sockets. The server manages users, sessions, and a virtual filesystem rooted at a specified directory. The client connects, logs in as a user, and sends commands. The server executes the commands within the root sandbox and returns results.

2) Q: How do you ensure that a user cannot escape the server root or their home directory?
   A: I normalize every path (handling absolute/relative, `.` and `..`) against the server root, then reject any resolved path that is outside the root. For user operations, I also check that the resolved path stays within the user’s home; only `list` may access other users’ homes if group permissions allow.

3) Q: How is `create_user` implemented and why might you need root?
   A: `create_user` creates a home directory and sets its ownership and permissions. If I create actual system users, I must run `adduser --disabled-password` via fork/exec, which needs root privileges. I drop privileges immediately after the necessary operations to minimize time as root.

4) Q: Why are `system()` and `popen()` prohibited, and how do you replace them?
   A: They spawn shells and are unsafe. I use `fork()` + `exec()` and `pipe()`/`dup2()` when I need to run allowed tools (e.g., `adduser`). For filesystem operations I use syscalls like `open`, `read`, `write`, `mkdir`, `rename`, `unlink`, `chmod`.

5) Q: How do you handle concurrent access to files?
   A: I use per-file synchronization (e.g., read-write locks) to allow multiple readers but exclusive writers. Reads block writes, writes block reads and other writes, and delete is treated as a write. Locks are keyed by canonical file path.

6) Q: What happens if two clients try to write to the same file at the same time?
   A: Only one writer can proceed at a time due to the exclusive lock. The other writer blocks or fails with a clear error, depending on my design. This prevents corruption.

7) Q: How is background upload/download implemented?
   A: The client spawns a worker process or thread to handle the transfer. The main client continues to accept commands. When the transfer finishes, the background worker prints a completion message in the specified format.

8) Q: How do you ensure the client doesn’t exit while background tasks are running?
   A: The client tracks active background transfers. On `exit`, it checks this count; if nonzero, it prints a warning and returns to the prompt instead of exiting.

9) Q: How do you implement `read -o set=<num>` and `write -o set=<num>`?
   A: I parse the offset option; on the server I call `lseek(fd, offset, SEEK_SET)` before reading or writing the file data. For write, I create the file with 0700 if it does not exist.

10) Q: What is meant by logical size in `list`?
    A: I interpret logical size as the file size in bytes from `stat` (st_size). For directories I can show 0 or the directory entry size, but the safest is to show st_size.

11) Q: How do you handle relative paths sent by the client?
    A: I maintain a current working directory per session. Relative paths are resolved against it, and then normalized and checked against the sandbox before any operation.

12) Q: What is the difference between the real filesystem and the virtual filesystem in this project?
    A: The virtual filesystem is a real directory tree on disk, but the server exposes only the portion under the root directory and enforces per-user sandboxing and permissions as required by the spec.

13) Q: What errors do you report to the client?
    A: I report invalid commands, missing arguments, permission denied, path outside sandbox, file not found, and I/O errors. I return clear messages so the user doesn’t work blindly, as required.

14) Q: How do you avoid zombie processes if you fork for clients or background tasks?
    A: The server reaps children using `waitpid` with `SIGCHLD` handling or by joining worker threads. The client also reaps background workers to avoid zombies.

15) Q: How do you support multiple concurrent clients on the server?
    A: The server accepts connections and forks a process per client (or uses a process pool). Each client session is isolated, while shared resources are protected with synchronization.

16) Q: How do you ensure two clients can read the same file simultaneously?
    A: The read-write lock allows multiple readers to acquire the lock concurrently. Writers are blocked until all readers release the lock.

17) Q: What is your approach to permissions given the “same GROUP” rule?
    A: I set group ownership consistently for all users and rely on group permission bits. I ignore OTHER permissions as specified. Access checks use owner/group bits only.

18) Q: How do you implement `move <path1> <path2>`?
    A: I resolve both paths within the sandbox, then use `rename()` if same filesystem. If rename fails due to cross-filesystem constraints, I copy and unlink, but within the same root it should be on the same filesystem.

19) Q: How do you handle `list` across other users’ homes?
    A: I allow listing paths within the server root even if outside the user’s home, but only if group permissions allow it. I still prohibit traversal outside the root.

20) Q: How do you handle unexpected client disconnects?
    A: The server detects EOF on the socket, cleans up session state, releases any locks held by the session, and terminates the handler process cleanly.

21) Q: How do you implement the transfer_request feature for honors?
    A: The sender issues `transfer_request <file> <dest_user>`. The server generates an ID, notifies the dest user in real time, and blocks the sender if the dest user is offline. The dest user can `accept <dir> <ID>` to copy the file into their directory or `reject` to cancel, and both parties are notified.

22) Q: How do you avoid race conditions between transfer_request and normal file operations?
    A: I reuse the same locking mechanism. The source file is locked for reading during transfer, and the destination path is locked for writing, preventing concurrent writes or deletes.

23) Q: How do you format protocol messages between client and server?
    A: I use a simple line-based protocol with command tokens and length-prefixed payloads for file data. This avoids ambiguity and makes parsing reliable.

24) Q: What syscalls are essential in your implementation?
    A: `socket`, `bind`, `listen`, `accept`, `connect`, `recv`, `send`, `fork`, `exec`, `waitpid`, `open`, `read`, `write`, `close`, `lseek`, `mkdir`, `stat`, `chmod`, `chown`, `rename`, `unlink`.

25) Q: How do you guarantee the project compiles on Ubuntu 24.04?
    A: I rely only on standard C/POSIX APIs, avoid external libraries, and include a Makefile that builds both server and client using `gcc` with appropriate flags. I test in a Ubuntu 24.04 environment.

26) Q: Explain how you parse options like `-d` or `-b`.
    A: I tokenize the command line, scan for known flags, validate argument count, and then dispatch to handlers with a structured command object that includes flags and parameters.

27) Q: What happens if a user tries to access `..` to escape their home?
    A: Path normalization resolves `..`, and if the resulting canonical path is outside the user home or server root, the operation is denied with a clear error.

28) Q: How do you report progress or completion for background transfers?
    A: The background worker prints a completion line exactly as required: `[Background] Command: upload <server path> <client path> concluded` or the download equivalent.

29) Q: Why is modularity required and how did you structure the code?
    A: Modularity reduces duplication and makes testing and reasoning easier. I split the code into modules for networking, session/auth, filesystem ops, transfer, and utilities, each with its own header.

30) Q: How do you handle server shutdown when `exit` is received?
    A: The server validates the command, closes the listening socket, signals/terminates client handlers, and exits. If the spec allows immediate termination, I ensure resources are freed and children reaped to prevent zombies.

31) Q: How do you prevent data corruption during simultaneous read/write?
    A: Writers acquire exclusive locks and readers acquire shared locks. This prevents writing while reading and disallows concurrent writes.

32) Q: How do you handle missing arguments or invalid commands?
    A: I validate the command syntax early and return a descriptive error message without terminating the connection, per the error handling requirements.

33) Q: What is the risk if you keep root privileges for too long?
    A: It increases the attack surface and potential damage if there is a bug. Dropping privileges after the required operations adheres to least privilege.

34) Q: How do you implement `write` if the file doesn’t exist?
    A: The server creates it with `open(path, O_CREAT|O_WRONLY, 0700)` and then writes the client data, optionally at the specified offset.

35) Q: What is your strategy for client/server protocol errors mid-transfer?
    A: I detect short reads/writes or socket errors, send an error response, clean up partial files if needed, and keep the server running for other clients.

36) Q: How do you handle listing permissions and sizes in `list`?
    A: I use `stat` to get mode and size, then format permissions in octal or rwx form. I include each entry’s permissions and size as required.

37) Q: How do you deal with multiple logins of the same user?
    A: Each session maintains its own current directory and state. File access is synchronized globally, so concurrent operations from the same user are safe.

38) Q: Explain why you chose forked processes vs threads.
    A: Forked processes isolate client state and faults better, simplifying concurrency. Threads are lighter but require more careful synchronization of shared state. Either approach is acceptable if implemented safely.

39) Q: What do you do if `create_user` is called for an existing user?
    A: I check for existing user directory/user account and return a clear error without overwriting or changing existing permissions.

40) Q: How do you ensure the server root is created if it doesn’t exist?
    A: At startup, the server checks with `stat` and calls `mkdir` (and possibly `mkdir -p` logic) to create it before accepting clients.
