# Collaborative Network File System

## Group Details

| Member |       Name        | Roll No. |                    Role                           |
|--------|-------------------|----------|---------------------------------------------------|
|   1    | SNIGDHA PAUL      | 2405979  | Server-side implementation and client connections |
|   2    | ANKITA DAS        | 24051239 | Client-side implementation and file operations    |
|   3    | SHREYASHI NOOPUR  | 24051054 | Concurrency and conflict-resolution mechanism     | 
|   4    | SAMRIDDHI AGRAWAL | 24051572 | Synchronization, testing and documentation        |

---

## Project Choice

We have selected **Project 4: Collaborative Network File System**.

The project focuses on building a server-backed file system in which multiple clients can access and modify the same file concurrently. It involves important Computer Networks and distributed-systems concepts such as client-server communication, concurrent access, synchronization and consistency.

We chose this project because it provides an opportunity to understand how multiple clients communicate with a central server and how concurrent modifications to shared data can be handled in a controlled and predictable manner.

---

## Planned Features

The final project will include the following features:

- A server that maintains the shared file.
- Support for at least three clients connected to the same file simultaneously.
- Clients can open, edit and save the shared file.
- Changes made by one client are reflected on the server and propagated to other connected clients.
- A defined strategy for handling conflicting concurrent edits.
- Prevention of silent loss or corruption of edits during normal concurrent use.
- Handling of client disconnection and reconnection.
- Restoration of a consistent file view when a client reconnects.
- Demonstration of a deliberate concurrent-edit/conflict scenario.
- Basic error handling for communication and connection failures.

---

## Conflict-Resolution Strategy

We plan to implement a clear and predefined strategy for handling simultaneous conflicting edits.

The server will act as the central source of truth and will control how conflicting updates are accepted and propagated to connected clients. The exact mechanism will be implemented and documented as the project progresses.

---

## AI Usage Declaration

AI will be used only as a learning and development assistance tool.

We plan to use AI for:

- Understanding Computer Networks and socket-programming concepts required for the project.
- Clarifying concepts related to client-server communication, concurrency, synchronization and consistency.
- Understanding compiler errors, runtime errors and debugging issues.
- Getting suggestions for testing and identifying possible edge cases.
- Reviewing small portions of our own code for errors or improvements.
- Improving the clarity and structure of project documentation.
- Preparing for technical questions related to our implementation and design decisions.

AI will **not** be used to generate the complete project independently. The group members will be responsible for the project's design, implementation, integration, testing and understanding of the final code.

---

## Expected Outcome

The final system will demonstrate reliable communication between multiple clients and a central server, synchronized modification of a shared file, and predictable handling of concurrent edits, disconnections and reconnections.
