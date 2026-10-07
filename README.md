# IE3090 Network Programming – RemoteOps

## Student Details

- Registration Number: IT24103506
- Name: Siriwardana S.A.D.V.I
- Module: IE3090 – Network Programming
- Project: RemoteOps
- Session ID (SID): 6053
- TCP Port: 9410
- Authentication Token: OPS-3506

---

## 1. Project Overview

RemoteOps is a network programming application consisting of a RemoteOps Agent and a Controller.

The Agent operates as a TCP server and accepts connections from Controllers. The Controller communicates with the Agent using the defined TCP protocol.

The system supports authentication, system information retrieval, process listing, restricted command execution, file transfer, concurrent connections, and UDP monitoring.

The application is implemented in C using POSIX/BSD socket APIs on Linux.

---

## 2. Project Files

| File | Description |
|---|---|
| agent_506.c | RemoteOps Agent TCP server |
| controller_506.c | RemoteOps Controller TCP client |
| Makefile_506 | Build configuration |
| README.md | Project documentation |

---

## 3. Personalisation

| Item | Value |
|---|---|
| Registration Number | IT24103506 |
| TCP Port | 9410 |
| Session ID | 6053 |
| Authentication Token | OPS-3506 |
| Log File | remoteops_IT24103506.log |
| Storage Directory | ./agentfiles/IT24103506/ |

---

## 4. Main Features

### Authentication

The Controller must authenticate before protected commands can be executed.

Example:

    AUTH OPS-3506

Unauthenticated commands are rejected by the Agent.

### SYSINFO

SYSINFO provides:

- CPU load
- Memory usage
- System uptime

Example response:

    OK SYSINFO <cpu> <memory> <uptime> SID:6053

### LISTPROC

LISTPROC provides a snapshot of running processes on the Agent system.

### EXEC

EXEC supports only the required command whitelist:

    DATE
    UPTIME
    DISKFREE
    HOSTNAME
    WHOAMI

Arbitrary shell commands are not permitted.

### PUT

PUT uploads a file from the Controller to the Agent.

### GET

GET downloads a file from the Agent to the Controller.

### MONITOR

MONITOR provides periodic SYSINFO updates through UDP.

### QUIT

QUIT terminates the Controller session.

---

## 5. Supported Commands

| Command | Purpose |
|---|---|
| AUTH <token> | Authenticate Controller |
| SYSINFO | Retrieve system information |
| LISTPROC | Retrieve process snapshot |
| EXEC <command> | Execute whitelisted command |
| PUT <filename> <filesize> | Upload file |
| GET <filename> | Download file |
| MONITOR START <udp_port> | Start UDP monitoring |
| MONITOR STOP | Stop UDP monitoring |
| QUIT | Terminate session |

---

## 6. TCP Protocol Handling

The implementation follows the required line-based TCP protocol.

- Commands are terminated using a newline.
- Responses contain the personalised SID.
- Fragmented TCP commands are handled.
- Coalesced TCP commands are handled.
- File transfers use exact byte counts.
- PUT receives the specified number of bytes.
- GET sends the exact requested file size.

The implementation was tested using both fragmented and coalesced TCP commands.

---

## 7. Concurrent Connections

The Agent uses POSIX threads to handle multiple Controller connections concurrently.

Each accepted Controller connection is handled by a separate worker thread.

The implementation was tested using five simultaneous Controller connections.

---

## 8. UDP Monitoring

UDP monitoring is started using:

    MONITOR START 9000

The Agent periodically sends SYSINFO datagrams to the specified UDP port.

Monitoring can be stopped using:

    MONITOR STOP

Monitoring also stops when the Controller disconnects or sends QUIT.

---

## 9. File Transfer

### PUT

The Controller sends:

    PUT <filename> <filesize>

followed immediately by the exact number of file bytes.

Uploaded files are stored in:

    ./agentfiles/IT24103506/

### GET

The Controller sends:

    GET <filename>

The Agent returns the file information followed by the exact file bytes.

### Transfer Throughput

The Controller reports the approximate transfer speed after successful PUT and GET operations.

Example:

    PUT throughput: 703.21 bytes/sec
    GET throughput: 5353.09 bytes/sec

---

## 10. Logging

Agent activity is recorded in:

    remoteops_IT24103506.log

The log records events such as:

- Agent startup
- Controller connections
- Authentication
- Commands
- File transfers
- Session termination

Log entries contain timestamps.

Example:

    [2026-10-06 13:43:49] Agent started
    [2026-10-06 13:44:19] Controller connected
    [2026-10-06 13:44:19] AUTH successful
    [2026-10-06 13:44:27] SYSINFO command

---

## 11. Error Handling

The Agent handles invalid and unauthorised requests.

Examples include:

    ERR 003 NOT_AUTHENTICATED SID:6053
    ERR 002 COMMAND_NOT_ALLOWED SID:6053
    ERR 005 FILE_NOT_FOUND SID:6053
    ERR 004 FILE_TOO_LARGE SID:6053

The EXEC command is restricted to the predefined whitelist.

---

## 12. Compilation

### Agent

    gcc -Wall -Wextra -std=c11 -pthread agent_506.c -o agent

### Controller

    gcc -Wall -Wextra -std=c11 -pthread controller_506.c -o controller

### Makefile

The Agent can also be compiled using:

    make -f Makefile_506

---

## 13. Running the System

Start the Agent:

    ./agent

The Agent starts on TCP port 9410 with SID 6053.

Start the Controller from another terminal:

    ./controller

The Controller then connects to the Agent.

---

## 14. Testing Performed

The following functionality was tested:

- Authentication
- SYSINFO
- LISTPROC
- EXEC whitelist
- PUT
- GET
- File transfer throughput
- Five concurrent Controllers
- TCP fragmented commands
- TCP coalesced commands
- UDP monitoring
- UDP monitoring termination
- Missing file handling
- Invalid file-size handling
- Unauthenticated command rejection
- Timestamped logging

---

## 15. Project Structure

    IE3090-RemoteOps-IT24103506/
    |
    +-- agent_506.c
    +-- controller_506.c
    +-- Makefile_506
    +-- README.md
    +-- DESIGN_DIARY.md
    +-- AI_PROMPT_LOG.md
    +-- REFLECTION.md
    +-- .gitignore

Runtime files generated during execution include:

    agent
    controller
    remoteops_IT24103506.log
    agentfiles/IT24103506/

---

## 16. Optional Extension

File transfer throughput measurement was implemented as an additional feature.

The Controller calculates and displays the approximate transfer speed after successful PUT and GET operations.

Example:

    PUT throughput: 703.21 bytes/sec
    GET throughput: 5353.09 bytes/sec

---

## 17. GitHub Repository

GitHub repository:

    https://github.com/VimukthiSiriwardana/IE3090-RemoteOps-IT24103506

The repository contains the project source code and incremental Git development history.

---

## 18. Platform and Tools

- Operating System: Linux / CentOS
- Programming Language: C
- Compiler: GCC
- Networking: TCP and UDP sockets
- Concurrency: POSIX threads
- Development Environment: SSH / Linux terminal
- Version Control: Git and GitHub

---

## 19. Development Approach

The project was developed incrementally.

The main development stages were:

1. Agent server foundation
2. SYSINFO
3. LISTPROC
4. EXEC whitelist
5. GET file transfer
6. Concurrent Controller handling
7. UDP monitoring
8. PUT and GET transfer handling
9. Transfer throughput reporting
10. Timestamped activity logging
11. Testing and documentation

Git was used to maintain the development history through incremental commits.

---

## 20. Security Considerations

The implementation includes the following security controls:

- Authentication before protected operations
- Personalised authentication token
- Restricted EXEC command whitelist
- No arbitrary shell command execution
- File transfer size validation
- File-not-found handling
- Unauthenticated command rejection
- Timestamped activity logging
- Personalised SID in responses

---

## 21. Project Status

The RemoteOps implementation provides the required core functionality:

- TCP client/server communication
- Authentication
- SYSINFO
- LISTPROC
- Restricted EXEC
- PUT
- GET
- Concurrent connections
- UDP monitoring
- TCP protocol framing
- Error handling
- Timestamped logging
- Transfer throughput measurement

The project is maintained using Git and GitHub.

---

**IE3090 Network Programming – RemoteOps**

**Registration Number:** IT24103506

**SID:** 6053
