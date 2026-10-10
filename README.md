----------------------------------------------------------------------------------------------------------------------------------

## Collaborative Network File System

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
- Concurrent updates are handled using an operation-based collaborative editing mechanism. Clients send INSERT and DELETE operations with their base revision, and the server transforms concurrent operations before applying them to the authoritative document.
- A stale client cannot silently overwrite a newer update.
- A client that disconnects can reconnect and obtain the latest version of the shared file.

The system demonstrates important Computer Networks concepts including TCP socket programming, client-server communication, concurrent clients, synchronization, versioning, and consistency.

---

## 2. System Architecture

The system follows a centralized client-server architecture.

                    +---------------------------+
                    |        SERVER             |
                    |                           |
                    |  Documents                |
                    |  Revisions                |
                    |  Operation History        |
                    |  Operation Transformation |
                    |  Client Management        |
                    +----------+----------------+
                               |
                 +-------------+-------------+
                 |             |             |
                 |             |             |
            +----+----+   +----+----+   +----+----+
            | Client 1|   | Client 2|   | Client 3|
            +---------+   +---------+   +---------+
The server acts as the central source of truth.
The server maintains the authoritative document state and orders accepted editing operations so that connected clients can remain synchronized.

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

- Maintaining multiple documents.
- Maintaining the current revision of each document.
- Maintaining the history of accepted editing operations.
- Receiving INSERT and DELETE operations from clients.
- Transforming incoming operations against previously accepted operations.
- Applying transformed operations to the authoritative document.
- Incrementing the document revision after each accepted operation.
- Sending acknowledgements to the editing client.
- Broadcasting accepted operations to other connected clients.
- Handling client disconnections.

Client

The client is responsible for:

- Maintaining a local copy of the currently opened document.
- Maintaining the current document revision.
- Sending INSERT and DELETE operations to the server.
- Processing acknowledgements for its own operations.
- Receiving editing operations from other clients.
- Applying live INSERT and DELETE operations locally.
- Keeping the local document synchronized with the server revision.
- Detecting revision gaps and requesting resynchronization when necessary.

## 5. Communication Protocol

The client and server communicate using a simple application-level protocol over TCP.

Client Requests

LIST

CREATE <name>

OPEN <name>

EDIT <file_name> <base_revision> INSERT <position> <length>
<text payload>

EDIT <file_name> <base_revision> DELETE <position> <length>

QUIT

## 6. Server Responses

WELCOME

FILES <count>

DOC <name> <revision> <size>
<document contents>

CREATE_OK

ACK <name> <revision> <position> <length>

OP <name> <revision> <sender> INSERT <position> <length>
<text payload>

OP <name> <revision> <sender> DELETE <position> <length>

ERROR <message>

BYE

## 7. Operation-Based Collaborative Editing

The system uses an operation-based collaborative editing mechanism.

Instead of sending the complete modified file, a client sends an
editing operation to the server.

Two operation types are supported:

- INSERT
- DELETE

Each operation contains the document name, the client's base
revision, operation position, and operation length. INSERT operations
also contain the inserted text.

The server is the authoritative source of truth. When an operation
arrives, the server compares its base revision with the current
document revision.

If previous operations have already been accepted, the incoming
operation is transformed against the relevant operation history.

The transformed operation is then applied to the authoritative
document.

After applying the operation:

1. The document revision is incremented.
2. The accepted operation is stored in the operation history.
3. An ACK is sent to the originating client.
4. The operation is broadcast to other connected clients.

Other clients apply the received operation to their local copy,
allowing edits to appear as live updates.

## 8. Consistency Model

The server acts as the authoritative source of truth for all
documents.

Each client maintains a local copy of the currently opened document
and its revision number.

Clients submit editing operations based on the revision they last
observed.

The server orders accepted operations and transforms incoming
operations against previously accepted operations when necessary.

This allows concurrent edits to be incorporated into the shared
document instead of simply rejecting every operation based on a stale
revision.

Accepted operations are propagated to other connected clients so that
their local copies remain synchronized.

## 9. Update Propagation

When a client successfully modifies the shared file:

Client 1
   |
   | EDIT ... INSERT/DELETE
   v
Server
   |
   | Transform operation
   v
Apply to authoritative document
   |
   | Revision++
   +------------+
   |            |
  ACK           OP
   |            |
Client 1     Client 2 / Client 3

The new server explicitly sends an ACK to the originating client and broadcasts an OP to other clients.

## 10. Client Disconnection and Reconnection

If a client disconnects, the server removes the disconnected client from the active client set.

Other clients can continue working with the shared file.

If the disconnected client later reconnects, it requests the current shared file from the server.

## 11. Conflict Handling Example

Suppose both Client 1 and Client 2 have revision 5.

Client 1 performs:

INSERT at position 5

Client 2 performs:

INSERT at position 10

Both operations are submitted using base revision 5.

The server accepts one operation first and increments the document
revision.

When the second operation arrives, the server transforms its position
relative to the already accepted operation.

The transformed operation is then applied to the document.

The server sends an ACK to the originating client and broadcasts the
accepted operation to the other clients.

Therefore, both edits can be incorporated into the shared document
without simply rejecting the second client's edit.

## 12. Testing Performed

The following scenarios have been tested:

Test 1: Client Connection
Server starts successfully and multiple clients connect through TCP.

Test 2: File Listing
Clients can request and view the available documents.

Test 3: File Creation
A client can create a new document on the server.

Test 4: File Opening
Clients can open and receive the current document and revision.

Test 5: Insert Operation
A client inserts text and the server accepts the operation.

Test 6: Delete Operation
A client deletes text and the server applies the operation.

Test 7: Live Operation Propagation
An operation performed by one client is received and applied by
other connected clients.

Test 8: Simultaneous Editing
Multiple clients perform edits on the same document while connected.
The server processes the operations and maintains a consistent
document state.

Test 9: Revision Tracking
Document revisions increase after accepted operations.

Test 10: Three-Client Operation
Three clients are connected to the same server and tested with
collaborative editing operations.

Test 11: Disconnect and Reconnect
A disconnected client can reconnect and obtain the current document
state.

## 13. Limitations

Current limitations include:

- The implementation is a simplified academic collaborative editing
  system rather than a full production-grade Google Docs implementation.
- The maximum document size is limited by the configured buffer size.
- The client interface is terminal based.
- The system supports INSERT and DELETE operations rather than a
  complete character-level editor with cursor synchronization.
- The server acts as the central authority for document state.

## 14. How to Compile

Compile the server:

gcc server_final.c -o Server

Compile the client:

gcc client_final.c -o Client

## 15. How to Run

Step 1: Start the Server
Open a terminal:
./Server

Step 2: Start Client 1
Open another terminal:
./Client

Step 3: Start Client 2
Open another terminal:
./Client

Step 4: Start Client 3
Open another terminal:
./Client

The three clients can then access the same shared file through the server.

## 16. Project Structure
collaborative_network_file_system/
│
├── server_final.c
├── client_final.c
├── documents
|	|--- shared.txt
├── README.md
└── README_writeup.md
File Description
File	Description
server_final.c	Server-side implementation
client_final.c	Client-side implementation
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
Operation-based collaborative editing
Synchronization
Disconnect/reconnect handling
Git-based collaborative development

The system uses an operation-based collaborative editing mechanism.

Instead of sending the complete modified file, a client sends an
editing operation to the server.

Two operation types are supported:

- INSERT
- DELETE

Each operation contains the document name, the client's base
revision, operation position, and operation length. INSERT operations
also contain the inserted text.

The server is the authoritative source of truth. When an operation
arrives, the server compares its base revision with the current
document revision.

If previous operations have already been accepted, the incoming
operation is transformed against the relevant operation history.

The transformed operation is then applied to the authoritative
document.

After applying the operation:

1. The document revision is incremented.
2. The accepted operation is stored in the operation history.
3. An ACK is sent to the originating client.
4. The operation is broadcast to other connected clients.

Other clients apply the received operation to their local copy,
allowing edits to appear as live updates.

----------------------------------------------------------------------------------------------------------------------------------
