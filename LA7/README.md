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
sudo ./cldp_client
[+] Socket created successfully
[+] Socket options set to SO_BROADCAST
[+] Socket options set to IP_HDRINCL
[+] Socket options set to SO_RCVBUF (64 KB)
[+] CLDP client started - listening for HELLO messages...

[+] Received 28 bytes from 10.145.123.171
[+] Received HELLO from 10.145.123.171. [Packet size = 28 bytes]
[+] Sent QUERY to server 10.145.123.171 [Size = 28  | Transaction ID = 32806]
[+] Received 28 bytes from 10.145.123.171
[+] Received 268 bytes from 10.145.123.171
[+] Received RESPONSE from 10.145.123.171. [Packet size = 268 bytes, Transaction ID = 32806]

Metadata:

Hostname       : tuhin-hp15s-fy5xxx
CPU Load       : avg load: 0.54, loads: 0.55, 0.63, 0.45
System Time    : 19:22:30 2025-03-31
Memory Usage   : 8455.48 MB used / 15663.20 MB total (53.98%)
Uptime         : 0 days, 0 hours, 46 minutes, 53 seconds

[+] Received 28 bytes from 10.145.123.171
[+] Received HELLO from 10.145.123.171. [Packet size = 28 bytes]
[+] Sent QUERY to server 10.145.123.171 [Size = 28  | Transaction ID = 32807]
[+] Received 28 bytes from 10.145.123.171
[+] Received 267 bytes from 10.145.123.171
[+] Received RESPONSE from 10.145.123.171. [Packet size = 267 bytes, Transaction ID = 32807]

Metadata:

Hostname       : tuhin-hp15s-fy5xxx
CPU Load       : avg load: 0.51, loads: 0.47, 0.60, 0.44
System Time    : 19:22:40 2025-03-31
Memory Usage   : 8427.47 MB used / 15663.20 MB total (53.80%)
Uptime         : 0 days, 0 hours, 47 minutes, 3 seconds

[+] Received 28 bytes from 10.145.123.171
[+] Received HELLO from 10.145.123.171. [Packet size = 28 bytes]
[+] Sent QUERY to server 10.145.123.171 [Size = 28  | Transaction ID = 32808]
[+] Received 28 bytes from 10.145.123.171
[+] Received 268 bytes from 10.145.123.171
[+] Received RESPONSE from 10.145.123.171. [Packet size = 268 bytes, Transaction ID = 32808]

Metadata:

Hostname       : tuhin-hp15s-fy5xxx
CPU Load       : avg load: 0.47, loads: 0.39, 0.58, 0.44
System Time    : 19:22:50 2025-03-31
Memory Usage   : 8416.79 MB used / 15663.20 MB total (53.74%)
Uptime         : 0 days, 0 hours, 47 minutes, 13 seconds

[+] Received 28 bytes from 10.145.99.247
[+] Received HELLO from 10.145.99.247. [Packet size = 28 bytes]
[+] Sent QUERY to server 10.145.99.247 [Size = 28  | Transaction ID = 32809]
[+] Received 273 bytes from 10.145.99.247
[+] Received RESPONSE from 10.145.99.247. [Packet size = 273 bytes, Transaction ID = 32809]

Metadata:

Hostname       : diganta-hp240g8notebookpc
CPU Load       : avg load: 0.25, loads: 0.23, 0.29, 0.23
System Time    : 19:22:55 2025-03-31
Memory Usage   : 4638.30 MB used / 7610.58 MB total (60.95%)
Uptime         : 0 days, 0 hours, 37 minutes, 3 seconds

[+] Received 28 bytes from 10.145.123.171
[+] Received HELLO from 10.145.123.171. [Packet size = 28 bytes]
[+] Sent QUERY to server 10.145.123.171 [Size = 28  | Transaction ID = 32810]
[+] Received 28 bytes from 10.145.123.171
[+] Received 268 bytes from 10.145.123.171
[+] Received RESPONSE from 10.145.123.171. [Packet size = 268 bytes, Transaction ID = 32810]

Metadata:

Hostname       : tuhin-hp15s-fy5xxx
CPU Load       : avg load: 0.44, loads: 0.33, 0.56, 0.44
System Time    : 19:23:00 2025-03-31
Memory Usage   : 8398.83 MB used / 15663.20 MB total (53.62%)
Uptime         : 0 days, 0 hours, 47 minutes, 23 seconds

[+] Received 28 bytes from 10.145.99.247
[+] Received HELLO from 10.145.99.247. [Packet size = 28 bytes]
[+] Sent QUERY to server 10.145.99.247 [Size = 28  | Transaction ID = 32811]
[+] Received 274 bytes from 10.145.99.247
[+] Received RESPONSE from 10.145.99.247. [Packet size = 274 bytes, Transaction ID = 32811]

Metadata:

Hostname       : diganta-hp240g8notebookpc
CPU Load       : avg load: 0.24, loads: 0.19, 0.28, 0.23
System Time    : 19:23:05 2025-03-31
Memory Usage   : 4600.86 MB used / 7610.58 MB total (60.45%)
Uptime         : 0 days, 0 hours, 37 minutes, 13 seconds
```

This output sequence illustrates:
- The server broadcasting a HELLO message.
- The client sending a QUERY message upon receiving HELLO.
- The server returning a RESPONSE with the requested metadata.
- The client receiving and displaying the metadata.

---
