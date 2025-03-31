# Computer Networks - Lab Assignment 7

This repository implements a custom client-server application using a raw socket protocol called CLDP. The protocol is designed to query and respond with system metadata such as hostname, CPU load, system time, network status, and memory usage.

---

## Build and Run Instructions

### Prerequisites
- **Operating System:** Linux (raw sockets require Linux and sufficient privileges)
- **Compiler:** gcc
- **Tools:** make (optional)
- **Privileges:** Root or sudo privileges are required to run applications that use raw sockets.

### Building the Application

Compile the client and server using `gcc`:

   ```bash
    make all
    gcc -o cldp_client cldp_client.c
    gcc -o cldp_server cldp_server.c -lpthread

   ```

### Running the Applications

Since raw sockets are used, run the programs with elevated privileges:

- **Run the Server:**
  ```bash
  make runserver
  sudo ./cldp_server
  ```

- **Run the Client (in a separate terminal):**
  ```bash
  make runclient
  sudo ./cldp_client
  ```


## Protocol Format and Design Details

### Protocol Overview
CLDP is a custom protocol layered on top of raw IP packets. The protocol uses the IP protocol number **253** and manually constructs both the IP header and an attached CLDP header.

### CLDP Header Format

The custom CLDP header structure is defined as follows:

- **Type (1 byte):**  
  Specifies the message type:
  - `0x01`: HELLO – Server announces its availability.
  - `0x02`: QUERY – Client requests metadata.
  - `0x03`: RESPONSE – Server replies with the requested metadata.

- **Length (1 byte):**  
  Length (in bytes) of the payload data. (Primarily used in RESPONSE messages.)

- **Transaction ID (2 bytes):**  
  A unique identifier for matching queries with responses.

- **Reserved (4 bytes):**
  - In **QUERY** messages, this field indicates the requested metadata type: ALL_METADATA

### Packet Structure

Each packet is composed of:
1. **IP Header:**  
   Manually constructed with fields such as version, header length, total length, TTL, source and destination addresses, and a checksum calculated over the header.

2. **CLDP Header:**  
   Immediately follows the IP header, detailing the type, payload length, transaction ID, and reserved metadata type.

3. **Payload (for RESPONSE messages):**  
   Contains the requested metadata (e.g., hostname, CPU load, etc.).

### Design Flow

- **Server Operation:**
  - **HELLO Broadcast:** The server periodically (every 10 seconds) broadcasts a HELLO message to announce its presence on the network.
  - **Handling Queries:** Upon receiving a QUERY from a client, the server:
    - Extracts the metadata request type from the CLDP header.
    - Retrieves the corresponding system metadata (e.g., using system calls).
    - Constructs a RESPONSE JSON message, appending the metadata into the payload.
    - Sends the RESPONSE back to the client.

- **Client Operation:**
  - **Listening for HELLO:** The client listens for incoming HELLO messages.
  - **Sending a QUERY:** Once a HELLO is received, the client sends a QUERY message to the server.
  - **Receiving and Matching Response:** The client waits for a RESPONSE message that matches its pending transaction ID and then displays the received metadata.

---

## Assumptions and Limitations

- **Privilege Requirements:**  The application uses raw sockets which require root privileges to operate.

- **Platform Dependency:**  The implementation is designed and tested on Linux systems.

- **Broadcast Environment:** The protocol relies on broadcast messages (using the address `255.255.255.255`), assuming that the network environment permits broadcast communication.

- **Concurrency Limits:**  The client can handle up to 100 concurrent queries. If this limit is reached, further queries are not initiated until some responses are received.

- **Queries:** The client can only request for a fixed type of metadata. There is no option to customize the metadata we want to receive.
  

---

## Demo Output

Below is an example of the demo output captured during a session:

```plaintext
[+] Received 28 bytes from 10.145.123.171
[+] Received HELLO from 10.145.123.171. [Packet size = 28 bytes]
[+] Sent QUERY to server 10.145.123.171 [Size = 28 | Transaction ID = 14912]
[+] Received 28 bytes from 10.145.123.171
[+] Received 281 bytes from 10.145.123.171
[+] Received RESPONSE from 10.145.123.171. [Packet size = 281 bytes, Transaction ID = 14912]

Metadata:

Hostname       : tuhin-hp15s-fy5xxx
CPU Load       : avg load: 0.73, loads: 0.89, 0.68, 0.62
System Time    : 12:57:20 2025-03-31
Memory Usage   : 15256.09 MB used / 15663.21 MB total (97.40%)
Uptime         : 8 days, 11 hours, 55 minutes, 36 seconds
```

This output sequence illustrates:
- The server broadcasting a HELLO message.
- The client sending a QUERY message upon receiving HELLO.
- The server returning a RESPONSE with the requested metadata.
- The client receiving and displaying the metadata.

---
