# Makefile for CLDP Protocol Implementation
# Assignment 7

CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99
LDFLAGS = -pthread

# Executables
CLIENT = cldp_client
SERVER = cldp_server

# Default target
all: $(CLIENT) $(SERVER)

# Compile the client
$(CLIENT): cldp_client.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# Compile the server
$(SERVER): cldp_server.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# Run the server (requires sudo and interface parameters)
# Usage: make run-server IFACE=<interface_name> IP=<server_ip>
run-server: $(SERVER)
	@if [ -z "$(IFACE)" ] || [ -z "$(IP)" ]; then \
		echo "Error: Missing required parameters"; \
		echo "Usage: make run-server IFACE=<interface_name> IP=<server_ip>"; \
		exit 1; \
	fi
	sudo ./$(SERVER) $(IFACE) $(IP)

# Run the client (requires sudo and interface parameters)
# Usage: make run-client IFACE=<interface_name> IP=<client_ip> DEST=<destination_ip>
run-client: $(CLIENT)
	@if [ -z "$(IFACE)" ] || [ -z "$(IP)" ] || [ -z "$(DEST)" ]; then \
		echo "Error: Missing required parameters"; \
		echo "Usage: make run-client IFACE=<interface_name> IP=<client_ip> DEST=<destination_ip>"; \
		exit 1; \
	fi
	sudo ./$(CLIENT) $(IFACE) $(IP) $(DEST)

# Get the name of available network interfaces
ifconfig:
	ip -br link show

# Clean up
clean:
	rm -f $(CLIENT) $(SERVER) *.o

.PHONY: all clean run-server run-client ifconfig