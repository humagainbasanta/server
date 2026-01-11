# CSAP Project 2026 — Client/Server Virtual FS

This project implements a C client/server system that manages per-user virtual home
directories under a server root. Clients connect over sockets and issue commands.

## Build
```bash
make
```
Builds `Server` and `Client` in the project root.

## Run (step by step)
1) Start the server (choose a root directory).
```bash
./Server /tmp/csap_root 127.0.0.1 8080
```
The server listens on IP/port and creates the root directory if missing.

2) In a new terminal, start the client.
```bash
./Client 127.0.0.1 8080
```
You now have an interactive prompt (`client#`). Type `help` to see commands.

3) Create users (no password).
```bash
create_user alice 0770
create_user bob 0770
```
Creates user homes under the server root with permissions.

4) Log in and manage files.
```bash
login alice
create test.txt 0660
write test.txt
```
Type content, then press Enter twice to finish (Ctrl+D still works when piping).
To switch users, log out first:
```bash
logout
login bob
```

5) Read files.
```bash
read test.txt
read -o set=6 test.txt
```
Reads whole file or from a byte offset.

6) List directories.
```bash
list
list /alice
```
`list` can view other users' directories if permissions allow.

7) Check current user.
```bash
whoami
```
Shows the logged-in username.

7) Upload / download.
```bash
upload /tmp/local.txt uploaded.txt
download uploaded.txt /tmp/downloaded.txt
```
Upload sends a local file to the server; download saves it locally.

8) Background transfers.
```bash
upload -b /tmp/local.txt bg_up.txt
download -b bg_up.txt /tmp/bg_down.txt
```
Background completion prints: `[Background] Command: ... concluded`.

9) Exit client.
```bash
exit
```
If background jobs are running, the client stays open until they finish.

## Server shutdown
From any client:
```bash
exit
```
This terminates the server immediately.

## Notes
- All paths are sandboxed inside the server root; users cannot access outside.
- Operations accept absolute and relative paths, plus `.` and `..`.
- Offsets support `-offset=` and `-o set=` forms for `read` and `write`.

## Tests
```bash
bash scripts/test_requirements.sh
```
Runs the full requirement checks.

```bash
bash tests/run_tests.sh
```
Runs a smaller end-to-end harness.

## Submission zip
```bash
bash tools/make_submission_zip.sh
```
Creates `csap_project.zip` with source, README, and build script only.

## Commands and expected outputs
Below are minimal examples with typical server responses.

```bash
create_user alice 0770
```
Expected: `OK`

```bash
login alice
```
Expected: `OK`

```bash
logout
```
Expected: `OK`

```bash
whoami
```
Expected: `OK alice`

```bash
create test.txt 0660
```
Expected: `OK`

```bash
create -d dir 0770
```
Expected: `OK`

```bash
chmod test.txt 0660
```
Expected: `OK`

```bash
move test.txt moved.txt
```
Expected: `OK`

```bash
delete moved.txt
```
Expected: `OK`

```bash
cd dir
```
Expected: `OK`

```bash
list
```
Expected:
```
OK
<perm> <size> <name>
END
```

```bash
read test.txt
```
Expected:
```
OK <size>
<file bytes printed to stdout>
```

```bash
read -offset=6 test.txt
```
Expected:
```
OK <size>
<file bytes from offset>
```

```bash
write test.txt
```
Type content, finish with two empty lines. Expected: `OK <bytes_written>`

```bash
write -offset=5 test.txt
```
Type content, finish with two empty lines. Expected: `OK <bytes_written>`

```bash
upload /tmp/local.txt uploaded.txt
```
Expected: `OK`

```bash
download uploaded.txt /tmp/downloaded.txt
```
Expected: `OK`

```bash
upload -b /tmp/local.txt bg_up.txt
```
Expected background line:
`[Background] Command: upload bg_up.txt /tmp/local.txt concluded`

```bash
download -b bg_up.txt /tmp/bg_down.txt
```
Expected background line:
`[Background] Command: download bg_up.txt /tmp/bg_down.txt concluded`

```bash
transfer_request uploaded.txt bob
```
Expected: `OK <id>` and receiver sees `NOTICE TRANSFER <id> alice uploaded.txt`

```bash
accept . <id>
```
Expected: `OK`

```bash
reject <id>
```
Expected: `OK`

```bash
exit
```
Expected: `OK`
