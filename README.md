# Collaborative Network File System

A client-server based collaborative file system that allows multiple clients to access and modify a common file through TCP socket communication.

The server maintains the authoritative copy of the shared file and manages file updates, versioning, conflict detection, synchronization, and client connections.

---

## 1. Project Overview

The Collaborative Network File System allows multiple clients to work with the same shared file through a central server.

The system is designed so that:

- Multiple clients can connect to the server simultaneously.
- Clients can view the current shared file.
- Clients can edit and submit changes to the shared file.
- The server maintains the authoritative version of the file.
- Updates made by one client are propagated to other connected clients.
- Concurrent updates are handled using version-based conflict resolution.
- A stale client cannot silently overwrite a newer update.
- A client that disconnects can reconnect and obtain the latest version of the shared file.

The system demonstrates important Computer Networks concepts including TCP socket programming, client-server communication, concurrent clients, synchronization, versioning, and consistency.

---

## 2. System Architecture

The system follows a centralized client-server architecture.

                    +----------------------+
                    |        SERVER        |
                    |                      |
                    |  Shared File         |
                    |  File Version        |
                    |  Conflict Handling   |
                    |  Client Management   |
                    +----------+-----------+
                               |
                 +-------------+-------------+
                 |             |             |
                 |             |             |
            +----+----+   +----+----+   +----+----+
            | Client 1|   | Client 2|   | Client 3|
            +---------+   +---------+   +---------+
The server acts as the central source of truth.

Each client maintains a local copy of the shared file and the version number received from the server.

## 3. Technologies Used

Language: C

Networking: TCP/IP

Socket API: Linux/POSIX sockets

Multiplexing: select()

Operating System: Ubuntu/Linux

Compiler: GCC

Version Control: Git and GitHub

## 4. Main Components
Server

The server is responsible for:

Creating and binding the TCP socket.
Listening for client connections.
Accepting multiple clients.
Managing connected clients.
Maintaining the shared file.
Maintaining the current file version.
Sending the current file to clients.
Receiving client updates.
Checking update versions.
Detecting conflicts.
Broadcasting successful updates to other connected clients.
Handling client disconnections and reconnections.

Client

The client is responsible for:

Connecting to the server.
Receiving the current shared file.
Displaying the shared file.
Allowing the user to edit the file.
Sending file updates to the server.
Maintaining the locally known file version.
Processing successful update responses.
Processing updates made by other clients.
Handling conflict responses.
Updating its local file after a conflict.
Reconnecting and obtaining the latest server file.

## 5. Communication Protocol

The client and server communicate using a simple application-level protocol over TCP.

Client Requests
Get Current File
GET_FILE

The client requests the latest version of the shared file from the server.

Update File
UPDATE <version> <size>

The client sends the version it currently has along with the size of the updated file, followed by the file contents.

## 6. Server Responses
Welcome
WELCOME

Sent when a client successfully connects.

File Data
FILE_DATA <version> <size>

The server sends the current file version and file size, followed by the file contents.

Successful Update
UPDATE_OK <new_version>

Sent when the server accepts a client's update.

The server increments the file version after accepting the update.

File Update
FILE_UPDATE <version> <size>

Sent to other connected clients when one client successfully updates the shared file.

Conflict
CONFLICT <current_version> <current_size>

Sent when a client attempts to update the file using an outdated version.

The server also sends the current file contents so that the stale client can synchronize with the latest version.

Error
ERROR <message>

Used to report invalid requests or other communication errors.

## 7. Version-Based Conflict Resolution

The system uses version-based conflict resolution.

Each version of the shared file is identified by an integer version number.

For example:

Version 1
    |
    v
Client 1 updates
    |
    v
Version 2

A client sends the version number it last received when submitting an update.

The server compares:

Client Version
       vs
Server Version
Case 1: Versions Match

If:

Client Version == Server Version

the update is accepted.

The server:

Writes the new file.
Increments the version.
Sends UPDATE_OK to the updating client.
Broadcasts the new file to other connected clients.

Case 2: Versions Do Not Match

If:

Client Version != Server Version

the client has a stale copy.

The server rejects the update and sends:

CONFLICT <current_version> <current_size>

followed by the current file.

The client receives the latest file and updates its local copy.

This prevents a stale client from silently overwriting a newer update.

## 8. Consistency Model

The server acts as the authoritative source of truth for the shared file.

The client maintains:

A local copy of the file.
The version number of that copy.

The server maintains:

The authoritative shared file.
The current version number.

A client update is accepted only when the client is working with the current server version.

Therefore, stale updates are detected before they can overwrite newer data.

## 9. Update Propagation

When a client successfully modifies the shared file:

Client 1
   |
   | UPDATE
   v
Server
   |
   | Update accepted
   v
New File Version
   |
   +------------+------------+
   |            |            |
   v            v            v
Client 1      Client 2      Client 3

The server broadcasts the accepted update to the other connected clients.

This allows connected clients to synchronize their local copies with the server.

## 10. Client Disconnection and Reconnection

If a client disconnects, the server removes the disconnected client from the active client set.

Other clients can continue working with the shared file.

If the disconnected client later reconnects, it requests the current shared file from the server.

The server sends the latest:

FILE_DATA <version> <size>

and the file contents.

Therefore, the reconnecting client does not continue using an outdated version.

## 11. Conflict Handling Example

Suppose the server currently has:

Version = 5

Client 1 and Client 2 both previously received:

Version = 5

Client 1 submits an update first.

The server accepts it and changes the version to:

Version = 6

Client 2 still has:

Version = 5

If Client 2 now submits:

UPDATE 5 <size>

the server compares:

Client Version = 5
Server Version = 6

Since the versions do not match, the server detects a conflict.

Client 2 receives the latest file and is informed:

Conflict detected. Server has a newer version.

The stale update is not silently applied.

## 12. Testing Performed

The following scenarios have been tested:

Test 1: Client Connection
Server starts successfully.
Clients can connect through TCP.
Server accepts multiple clients.
Test 2: Shared File Retrieval
Clients request the current shared file.
Server sends the current file and version.
Test 3: File Update
A client edits the shared file.
The server accepts the update.
The file version is incremented.
Test 4: Update Propagation
One client updates the file.
Other connected clients receive the updated file.
Test 5: Concurrent Update Conflict
One client updates the file.
Another client attempts to update using an older version.
The server detects the conflict.
The stale update is rejected.
The latest server file is sent to the stale client.
Test 6: Conflict Payload Handling

The client correctly receives both:

CONFLICT <version> <size>

and the accompanying current file contents.

This prevents leftover file data from remaining in the TCP receive buffer and interfering with subsequent protocol messages.

Test 7: Client Disconnection
A connected client disconnects.
The server continues operating with the remaining clients.
Test 8: Reconnection
A previously disconnected client reconnects.
The client requests the current file.
The latest shared file and version are received.
Test 9: Three-Client Operation

Three clients have been connected to the server and tested with shared-file operations.

## 13. Limitations

The current implementation is designed as a compact academic demonstration of collaborative network file sharing.

Current limitations include:

The system operates on a single shared file.
The maximum file size is limited by the configured buffer size.
Conflict resolution is version-based rather than character-level collaborative editing.
The server acts as the central authority.
The client interface is terminal based.

The system is intended to demonstrate the networking, synchronization, consistency, and conflict-resolution concepts required by the project.

## 14. How to Compile

Compile the server:

gcc server.c -o server -pthread

Compile the client:

gcc client.c -o client

## 15. How to Run
Step 1: Start the Server

Open a terminal:

./server
Step 2: Start Client 1

Open another terminal:

./client
Step 3: Start Client 2

Open another terminal:

./client
Step 4: Start Client 3

Open another terminal:

./client

The three clients can then access the same shared file through the server.

## 16. Example Demonstration Flow

A typical demonstration can be performed as follows:

1. Start server.
2. Connect Client 1.
3. Connect Client 2.
4. Connect Client 3.
5. Display the shared file on all clients.
6. Edit the file from Client 1.
7. Show the update received by Clients 2 and 3.
8. Create a stale-version update from another client.
9. Show the conflict detection.
10. Show the latest file received by the stale client.
11. Disconnect a client.
12. Modify the file using another connected client.
13. Reconnect the disconnected client.
14. Show that it receives the latest shared file.

## 17. Project Structure
collaborative_network_file_system/
│
├── server.c
├── client.c
├── shared.txt
├── README.md
└── README_writeup.md
File Description
File	Description
server.c	Server-side implementation
client.c	Client-side implementation
shared.txt	Shared file used by the system
README.md	Project documentation
README_writeup.md	Initial project write-up/submission document

## 18. GitHub and Version Control

Git is used to maintain the development history of the project.

Development is performed incrementally, with changes committed as the project progresses.

The repository contains the source code, documentation, and development history of the project.

Compiled executable files such as:

client
server

are not part of the source repository.

## 19. Conclusion

The Collaborative Network File System demonstrates how multiple clients can communicate with a central server and work with a common shared file.

The project combines:

TCP socket programming
Client-server architecture
Multiple concurrent clients
Shared file management
Version tracking
Update propagation
Conflict detection
Synchronization
Disconnect/reconnect handling
Git-based collaborative development

The version-based conflict-resolution mechanism ensures that a client with an outdated file version cannot silently overwrite a newer version maintained by the server.

