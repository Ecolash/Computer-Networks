# Lightweight Custom Discovery Protocol (LCDP)

A custom network discovery protocol implemented using raw sockets in POSIX C.

## Protocol Description

LCDP is a lightweight protocol designed for discovering nodes on a local network and querying them for system information. The protocol uses raw IP sockets and a custom protocol number (253).

### Key Features

- Server nodes announce presence periodically via HELLO messages
- Clients can discover servers by listening for HELLO announcements
- Clients can query servers for system information
- Servers respond with hostname, timestamp, and system load metrics

### Message Types

1. **HELLO**: Announces node presence on the network
2. **QUERY**: Requests system information from a node
3. **RESPONSE**: Returns requested system information

## Building the Applications

### Prerequisites

- Linux-based operating system
- GCC compiler
- Root/sudo privileges (required for raw sockets)

### Compilation

To compile the server:

```bash
gcc -o lcdp_server lcdp_server.c -pthread