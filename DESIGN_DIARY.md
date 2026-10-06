# IE3090 Network Programming
# RemoteOps – Design Diary

**Student Registration Number:** IT24103506  
**Student:** Siriwardana S.A.D.V.I  
**Project:** RemoteOps  
**Module:** IE3090 – Network Programming  
**Repository:** IE3090-RemoteOps-IT24103506

---

## 1. Purpose of the Design Diary

This design diary records the major development decisions, implementation stages, testing activities, and improvements made during the development of the RemoteOps system.

The project was developed incrementally using Git. Each major development stage was committed separately so that the progress of the implementation could be tracked and reviewed.

The development focused on implementing the required TCP Agent/Controller architecture, authentication, system monitoring, process listing, command execution restrictions, file transfer, concurrent connections, UDP monitoring, logging, documentation, and additional functionality.

---

## 2. Personalisation Decisions

The assignment required the implementation to use personalised values derived from the student's registration number.

The following personalised values were used:

| Item | Value |
|---|---|
| Registration Number | IT24103506 |
| Numeric Registration Part | 24103506 |
| Agent TCP Port | 9410 |
| Session ID (SID) | 6053 |
| Authentication Token | OPS-3506 |
| Agent Source File | agent_506.c |
| Controller Source File | controller_506.c |
| Makefile | Makefile_506 |
| Log File | remoteops_IT24103506.log |
| Storage Directory | ./agentfiles/IT24103506/ |

These values were incorporated consistently throughout the Agent, Controller, logging, and file-storage implementation.

---

## 3. Development Approach

The RemoteOps application was developed incrementally rather than implementing all functionality at once.

The development process followed these general stages:

1. Establish the basic TCP Agent.
2. Add required system information functionality.
3. Add process listing.
4. Implement the restricted EXEC command functionality.
5. Implement GET file transfer.
6. Prepare the Agent for concurrent connections.
7. Implement concurrent Controller handling.
8. Implement UDP monitoring.
9. Implement Controller-side PUT and GET functionality.
10. Add transfer throughput measurement.
11. Improve activity logging with timestamps.
12. Create project documentation.
13. Improve the build process using Makefile.
14. Improve repository hygiene using `.gitignore`.

Each major stage was committed to Git with a descriptive commit message.

---

# 4. Development History

## 4.1 RemoteOps Agent Foundation

**Git Commit:** `a572e3a`  
**Commit Message:** `Initialize RemoteOps agent foundation`

### Development Decision

The first stage was to establish the basic Agent application and its TCP communication structure.

The Agent was configured to use the personalised TCP port and session identifier required for the project.

### Implementation

The initial Agent provided:

- TCP socket creation
- Socket binding
- Listening for Controller connections
- Personalised TCP port
- Personalised Session ID
- Authentication token configuration
- Basic connection handling

### Result

The RemoteOps Agent successfully started and listened on TCP port **9410**.

---

## 4.2 SYSINFO Implementation

**Git Commit:** `32674a4`  
**Commit Message:** `Implement SYSINFO system monitoring`

### Development Decision

SYSINFO was implemented as one of the main required monitoring commands.

The Agent needed to provide system information to the Controller.

### Implementation

SYSINFO was implemented to provide:

- CPU load information
- Memory information
- System uptime
- Session ID in the response

### Testing

The Controller was used to authenticate and request SYSINFO.

The Agent returned the expected system information successfully.

### Result

The SYSINFO functionality became operational and provided the required monitoring information.

---

## 4.3 LISTPROC Implementation

**Git Commit:** `140c1ba`  
**Commit Message:** `Implement LISTPROC process snapshot`

### Development Decision

A process snapshot was required, so process information was obtained from the Linux `/proc` filesystem.

### Implementation

The Agent was extended to inspect running processes and construct a process snapshot.

The implementation extracts relevant process information and returns it through the TCP connection.

### Testing

The LISTPROC command was tested through the Controller and successfully returned running process information.

### Result

The required LISTPROC functionality was implemented successfully.

---

## 4.4 EXEC Command Whitelist

**Git Commit:** `50dab29`  
**Commit Message:** `Implement EXEC command whitelist`

### Design Decision

Arbitrary shell command execution was avoided because allowing a Controller to execute unrestricted commands would introduce a major security risk.

Instead, the Agent was designed to accept only the commands explicitly permitted by the assignment.

### Allowed Commands

The implemented whitelist contains:

- DATE
- UPTIME
- DISKFREE
- HOSTNAME
- WHOAMI

### Error Handling

Commands outside the whitelist are rejected rather than executed.

### Testing

Both valid and invalid EXEC commands were tested.

An unauthorised command produced an appropriate error response instead of being executed.

### Result

The EXEC functionality was implemented using a restricted command whitelist.

---

## 4.5 GET File Transfer

**Git Commit:** `08c6284`  
**Commit Message:** `Implement GET file transfer`

### Development Decision

The Agent needed to support file downloading while preserving the assignment's exact byte-count transfer requirement.

### Implementation

The GET functionality was implemented so that:

1. The requested filename is received.
2. The Agent checks whether the file exists.
3. The file size is determined.
4. A response header is sent.
5. Exactly the required number of file bytes are transmitted.

### Error Handling

A missing file produces a `FILE_NOT_FOUND` error instead of attempting to transfer unavailable data.

### Result

GET file transfer was successfully implemented.

---

## 4.6 Preparation for Concurrent Connections

**Git Commit:** `10ec33a`  
**Commit Message:** `Prepare agent for pthread concurrency`

### Design Decision

The assignment requires the Agent to support multiple Controller connections simultaneously.

A pthread-based approach was selected to handle individual Controller connections independently.

### Implementation

The Agent was prepared to create worker threads for Controller connections.

This provided the foundation for simultaneous client handling.

### Result

The Agent architecture was prepared for concurrent Controller sessions.

---

## 4.7 Concurrent Controller Handling

**Git Commit:** `9da8642`  
**Commit Message:** `Implement concurrent controller handling`

### Development Decision

The Agent needed to support at least five simultaneous Controller connections.

A separate pthread was used to handle each accepted Controller connection.

### Implementation

The connection handling process was changed so that:

1. The Agent accepts a Controller connection.
2. A worker thread is created.
3. The worker handles authentication and commands.
4. The main Agent continues accepting new connections.

### Testing

Five Controller connections were opened simultaneously and successfully authenticated.

### Result

The Agent successfully demonstrated concurrent Controller handling.

---

## 4.8 UDP Monitoring

**Git Commit:** `4fc3cd5`  
**Commit Message:** `Implement UDP monitoring`

### Development Decision

The monitoring requirement uses UDP for periodic system information delivery.

A dedicated monitoring thread was used to send periodic SYSINFO datagrams.

### Implementation

The MONITOR functionality supports:

- MONITOR START
- Periodic UDP SYSINFO messages
- MONITOR STOP
- Session ID in monitoring data
- Termination of monitoring when the Controller disconnects or quits

### Testing

UDP monitoring was started using the Controller and periodic SYSINFO datagrams were observed.

The MONITOR STOP command was then tested and monitoring successfully terminated.

### Result

UDP monitoring functionality was successfully implemented and tested.

---

## 4.9 Controller PUT and GET

**Git Commit:** `8d27526`  
**Commit Message:** `Implement controller PUT and GET file transfer`

### Development Decision

After implementing the Agent-side GET functionality, the Controller required complete file transfer support.

### Implementation

The Controller was extended to support:

- PUT
- GET
- Exact file-size handling
- File transfer responses
- Local file writing
- Transfer completion reporting

### Testing

Files were successfully uploaded to the Agent and downloaded back through the Controller.

### Result

Both directions of file transfer became operational.

---

## 4.10 File Transfer Throughput

**Git Commit:** `1330b73`  
**Commit Message:** `Add file transfer throughput reporting`

### Development Decision

Transfer throughput was added as an additional useful feature to measure file transfer performance.

### Implementation

The Controller records the transfer duration and calculates the approximate transfer rate in bytes per second.

### Testing

A test file was uploaded and downloaded.

Example results included:

- PUT throughput: approximately **703 bytes/sec**
- GET throughput: approximately **5353 bytes/sec**

The exact throughput can vary depending on system load and test conditions.

### Result

The system can report transfer throughput after successful PUT and GET operations.

---

## 4.11 Timestamped Activity Logging

**Git Commit:** `4fc9888`  
**Commit Message:** `Add timestamps to activity logging`

### Development Decision

Activity logging was improved so that events could be associated with their execution time.

### Implementation

The Agent logging mechanism was extended to include timestamps in the format:

`YYYY-MM-DD HH:MM:SS`

The log records important activities such as:

- Agent startup
- Controller connections
- Successful authentication
- SYSINFO requests
- EXEC requests
- PUT operations
- QUIT operations

### Example

```text
[2026-10-06 13:43:49] Agent started
[2026-10-06 13:44:19] Controller connected
[2026-10-06 13:44:19] AUTH successful
[2026-10-06 13:44:27] SYSINFO command
[2026-10-06 13:44:31] EXEC command
[2026-10-06 13:44:40] PUT command
[2026-10-06 13:44:44] QUIT command
