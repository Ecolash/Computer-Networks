## Problem Statement

Build a non-blocking TCP server in C that accepts numbers from multiple clients, sorts the received numbers, and broadcasts the sorted list back to all clients. Each client sends exactly one integer, and the server waits for all clients to provide their numbers before processing and broadcasting the sorted result using the SO_BROADCAST socket option.

### System Overview

- **Connection Type:** Clients connect to the server via TCP.
- **Data Flow:**
    - Each client sends a single integer.
    - The server stores all received numbers.
    - Once all numbers are collected, the server sorts them.
    - The sorted list is broadcast to every client using the SO_BROADCAST option on the socket.
    
- **Key Focus Areas:**
    - Non-blocking socket handling.
    - TCP connection management.
    - Correct use of the SO_BROADCAST option.
    - Buffer management and reliable data delivery.
    - Modular code structure with robust error handling.

### Implementation Details

1. **Server:**
     - **Socket Setup:** Create a TCP socket configured to be non-blocking.
     - **Connection Handling:** Use mechanisms like poll or select to manage multiple client connections concurrently.
     - **Data Processing:**
         - Accept a connection and read a single integer from each client.
         - Collect the integers until numbers from all expected clients are received.
         - Sort the collected integers in ascending order.
     - **Broadcasting:**
         - Use the SO_BROADCAST enabled socket to send the sorted list to all connected clients.
     - **Error Handling:** Implement checks at every step such as socket creation, connection, data reception, and broadcast to ensure reliability.

2. **Client:**
     - **Connection Setup:** Establish a connection to the server using a TCP socket.
     - **Data Transmission:**
         - Send one integer to the server immediately upon connecting.
     - **Receiving Data:** Wait for the broadcast message containing the sorted list and display it.
     - **Error Handling:** Manage connection errors and handle unexpected disconnections.

3. **Non-blocking Operations:**
     - This approach prevents the server from getting stuck waiting for one client's data, allowing it to handle multiple clients simultaneously.
     - Using events (poll, select) to monitor the state of multiple sockets ensures responsiveness and efficient resource utilization.

### Compilation & Execution

1. **Compile the Server and Client:**
```bash
gcc server.c -o server
gcc client.c -o client
```

2. **Start the Server:**
```bash
./server
```

3. **Run Clients in Separate Terminals (example):**
```bash
./client 42
./client 7
./client 33
./client 10
./client 99
```

### Sample Outputs

**Server Output:**

- "Server started on port 12345."
- "Accepted connection from Client 1."
- "Received number: 42 from Client 1."
- "All numbers received. Sorting..."
- "Broadcasting sorted numbers: 7, 10, 33, 42, 99"

**Client Output (for each client):**

- "Connected to the server."
- "Sent number: 42" (or the respective integer)
- "Received sorted list: 7, 10, 33, 42, 99"

This solution demonstrates efficient and simultaneous handling of multiple client connections by using non-blocking socket operations and the SO_BROADCAST option for reliable delivery of the sorted data.